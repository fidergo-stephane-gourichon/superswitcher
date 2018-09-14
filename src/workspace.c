// Copyright (c) 2006 Nigel Tao.
// Licenced under the GNU General Public Licence (GPL) version 2.

#include "workspace.h"

#include <X11/X.h>

#include "draganddrop.h"
#include "screen.h"
#include "window.h"

//------------------------------------------------------------------------------

#define MINI_WORKSPACE_FONT_HEIGHT 16
#define MINI_WORKSPACE_WIDTH 48

//------------------------------------------------------------------------------

static void
workspace_remove_window_widgets (SSWorkspace *workspace)
{
  GList *children;
  GList *i;
  GtkWidget *child;
  GtkContainer *container;

  container = GTK_CONTAINER (workspace->window_container);
  children = gtk_container_get_children (container);
  for (i = children; i; i = i->next) {
    child = GTK_WIDGET (i->data);
    gtk_container_remove (container, child);
  }
  g_list_free (children);
}

//------------------------------------------------------------------------------

static void
workspace_add_window_widgets (SSWorkspace *workspace)
{
  GList *i;
  SSWindow *window;

  for (i = workspace->windows; i; i = i->next) {
    window = (SSWindow *) i->data;
    gtk_box_pack_start (GTK_BOX (workspace->window_container),
      window->widget, FALSE, FALSE, 0);
  }
}

//------------------------------------------------------------------------------

void
ss_workspace_add_window (SSWorkspace *workspace, SSWindow *window)
{
  if (window == NULL) {
    return;
  }
  workspace->windows = g_list_append (workspace->windows, window);
  gtk_box_pack_start (GTK_BOX (workspace->window_container),
    window->widget, TRUE, TRUE, 0);

  if (window->new_window_index != -1) {
    ss_workspace_reorder_window (workspace, window,
      window->new_window_index);
  }
}

//------------------------------------------------------------------------------

void
ss_workspace_remove_window (SSWorkspace *workspace, SSWindow *window)
{
  if (window == NULL) {
    return;
  }
  workspace->windows = g_list_remove (workspace->windows, window);
  gtk_container_remove (GTK_CONTAINER (workspace->window_container), window->widget);
}

//------------------------------------------------------------------------------

void
ss_workspace_reorder_window (SSWorkspace *workspace, SSWindow *window, int new_index)
{
  int n;
  if (window == NULL) {
    return;
  }
  n = g_list_length (workspace->windows);
  if (new_index < 0) {
    new_index = 0;
  }
  if (new_index > n) {
    new_index = n;
  }
  workspace->windows = g_list_remove (workspace->windows, window);
  workspace->windows = g_list_insert (workspace->windows, window, new_index);
  workspace_remove_window_widgets (workspace);
  workspace_add_window_widgets (workspace);
}

//------------------------------------------------------------------------------

static gboolean
on_scroll_event (GtkWidget *widget, GdkEventScroll *event, gpointer data)
{
  SSWorkspace *workspace;
  gboolean shifted;
  gboolean ctrled;

  workspace = (SSWorkspace *) data;
  shifted = ((event->state & GDK_SHIFT_MASK) == GDK_SHIFT_MASK);
  ctrled  = ((event->state & GDK_CONTROL_MASK) == GDK_CONTROL_MASK);

  switch (event->direction) {
  case GDK_SCROLL_UP:
  case GDK_SCROLL_LEFT:
    ss_screen_change_active_workspace_by_delta (workspace->screen, -1,
      shifted, ctrled, event->time);
    break;

  case GDK_SCROLL_DOWN:
  case GDK_SCROLL_RIGHT:
    ss_screen_change_active_workspace_by_delta (workspace->screen, +1,
      shifted, ctrled, event->time);
    break;

  default:
    g_assert_not_reached ();
  }
  return TRUE;
}

//------------------------------------------------------------------------------

static gboolean
on_button_press_event (GtkWidget *widget, GdkEventButton *event, gpointer data)
{
  SSWorkspace *workspace;
  workspace = (SSWorkspace *) data;
  ss_draganddrop_start (workspace->screen->drag_and_drop, NULL, workspace);
  return TRUE;
}

//------------------------------------------------------------------------------

static gboolean
on_button_release_event (GtkWidget *widget, GdkEventButton *event, gpointer data)
{
  SSWorkspace *workspace;
  SSScreen *screen;
  SSDragAndDrop *dnd;
  gboolean shifted;
  gboolean ctrled;
  GList *i;
  SSWindow *window;

  workspace = (SSWorkspace *) data;
  screen = workspace->screen;
  dnd = screen->drag_and_drop;

  shifted = ((event->state & GDK_SHIFT_MASK) == GDK_SHIFT_MASK);
  ctrled  = ((event->state & GDK_CONTROL_MASK) == GDK_CONTROL_MASK);

  if (dnd->is_dragging) {
    if (dnd->drag_workspace != NULL) {
      // This simple if clause is to avoid unnecessary work.
      if (dnd->drag_workspace != workspace) {
        for (i = workspace->windows; i; i = i->next) {
          // TODO - something weird is going on when dragging a workspace with
          // multiple windows - only the first one is being moved. Perhaps we're
          // modifying the list whilst iterating over it?
          window = (SSWindow *) i->data;
          ss_window_move_to_workspace (window, dnd->drag_workspace);
        }
      }
      if (window_manager_uses_viewports) {
        wnck_screen_move_viewport (screen->wnck_screen, screen->screen_width * dnd->drag_workspace->viewport, 0);
      } else {
        wnck_workspace_activate (dnd->drag_workspace->wnck_workspace, event->time);
      }
    }
  } else {
    // It's a plain old click, not a drag.
    ss_screen_change_active_workspace_to (workspace->screen,
      workspace->wnck_workspace, workspace->viewport, shifted, ctrled, event->time);
  }

  ss_draganddrop_on_release (dnd);
  return TRUE;
}

//------------------------------------------------------------------------------

static gboolean
on_motion_notify_event (GtkWidget *widget, GdkEventMotion *event, gpointer data)
{
  ss_draganddrop_on_motion (((SSWorkspace *) data)->screen->drag_and_drop);
  return TRUE;
}

//------------------------------------------------------------------------------

static void
on_expose_event_draw_text (GtkWidget *widget, SSWorkspace *workspace)
{
#ifdef HAVE_GTK_2_8
  cairo_t *c;
  cairo_text_extents_t extents;
  int x, y;

  c = gdk_cairo_create (widget->window);
  cairo_select_font_face (c, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
  cairo_set_font_size (c, MINI_WORKSPACE_FONT_HEIGHT);

  cairo_text_extents (c, workspace->title, &extents);
  x = (widget->allocation.width  - extents.width)  / 2;
  y = (widget->allocation.height + extents.height) / 2;

  cairo_set_source_rgba (c, 0, 0, 0, 0.25);
  cairo_move_to (c, x-1, y);
  cairo_show_text (c, workspace->title);
  cairo_set_source_rgba (c, 1, 1, 1, 0.25);
  cairo_move_to (c, x, y+1);
  cairo_show_text (c, workspace->title);
  cairo_destroy (c);
#endif
}

//------------------------------------------------------------------------------

static gboolean
on_expose_event (GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
  SSWorkspace *workspace;
  int screen_width, screen_height;
  double width_ratio, height_ratio;
  int w, h;
  int viewport_x;
  GList *i;
  SSWindow *active_window;
  WnckWindow *wnck_window;
  int state;
  GdkRectangle r;

  workspace = (SSWorkspace *) data;
  screen_width  = workspace->screen->screen_width;
  screen_height = workspace->screen->screen_height;
  active_window = workspace->screen->active_window;

  /* x = widget->allocation.x; */
  /* y = widget->allocation.y; */
  w = widget->allocation.width;
  h = widget->allocation.height;

  state = (workspace == workspace->screen->active_workspace) ? GTK_STATE_SELECTED : GTK_STATE_NORMAL;
  gdk_draw_rectangle (widget->window,
    widget->style->dark_gc[state], TRUE,
    1, 1, w-2, h-2);
  gdk_draw_rectangle (widget->window,
    widget->style->base_gc[state], FALSE,
    0, 0, w-1, h-1);

  width_ratio  = (double) w / (double) screen_width;
  height_ratio = (double) h / (double) screen_height;

  viewport_x = 0;
  if (window_manager_uses_viewports) {
    viewport_x = wnck_workspace_get_viewport_x (workspace->wnck_workspace);
  }

  for (i = workspace->screen->wnck_windows_in_stacking_order; i; i = i->next) {
    wnck_window = (WnckWindow *) i->data;
    if (wnck_window_get_workspace (wnck_window) != workspace->wnck_workspace) {
      continue;
    }
    if (wnck_window_is_minimized (wnck_window)) {
      continue;
    }

    state = GTK_STATE_ACTIVE;
    if ((active_window != NULL) &&
      ((wnck_window_get_xid (wnck_window) == wnck_window_get_xid (active_window->wnck_window)))) {
      state = GTK_STATE_SELECTED;
    }
    wnck_window_get_geometry (wnck_window, &r.x, &r.y, &r.width, &r.height);
    if (window_manager_uses_viewports) {
      r.x += viewport_x - (workspace->viewport * workspace->screen->screen_width);
    }

    r.x = 0.5 + r.x * width_ratio;
    r.y = 0.5 + r.y * height_ratio;
    r.width  = 0.5 + r.width  * width_ratio;
    r.height = 0.5 + r.height * height_ratio;

    if (r.width < 3) {
      r.width = 3;
    }
    if (r.height < 3) {
      r.height = 3;
    }

    gdk_draw_rectangle (widget->window,
      widget->style->bg_gc[state], TRUE,
      r.x+1, r.y+1, r.width-2, r.height-2);
    gdk_draw_rectangle (widget->window,
      widget->style->fg_gc[state], FALSE,
      r.x,   r.y,   r.width-1, r.height-1);
  }

  on_expose_event_draw_text (widget, workspace);
  return FALSE;
}

//------------------------------------------------------------------------------

int
ss_workspace_find_index_near_point (SSWorkspace *workspace, int x, int y)
{
  GList *i;
  int index;
  int n;
  SSWindow *window;
  GtkAllocation *a;

  n = g_list_length (workspace->windows);
  if (n == 0) {
    return -1;
  }

  for (i = workspace->windows, index = 0; i; i = i->next, index++) {
    window = (SSWindow *) i->data;
    a = &window->widget->allocation;
    if (y < (a->y + (a->height + WINDOW_ROW_SPACING) / 2)) {
      return index;
    }
  }
  return n;
}

//------------------------------------------------------------------------------

SSWorkspace *
ss_workspace_new (SSScreen *screen, WnckWorkspace *wnck_workspace, int viewport)
{
  SSWorkspace *w;
  GtkWidget *box;
  GtkWidget *align;
  GtkWidget *header;
  GtkWidget *align_h;
  GtkWidget *box_2;
  GtkWidget *align_2;

  box = gtk_vbox_new (FALSE, 3);
  align = gtk_alignment_new (0.5, 0.0, 0.0, 0.0);
  gtk_container_add (GTK_CONTAINER (align), box);

  header = gtk_drawing_area_new ();
  gtk_widget_add_events (header,
                         GDK_BUTTON_PRESS_MASK |
                         GDK_BUTTON_RELEASE_MASK |
                         GDK_POINTER_MOTION_HINT_MASK |
                         GDK_POINTER_MOTION_MASK);
  gtk_widget_set_size_request (header, MINI_WORKSPACE_WIDTH,
    MINI_WORKSPACE_WIDTH * screen->screen_aspect);

  align_h = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
  gtk_container_add (GTK_CONTAINER (align_h), header);
  gtk_box_pack_start (GTK_BOX (box), align_h, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (box), gtk_hseparator_new (), TRUE, TRUE, 0);

  box_2 = gtk_vbox_new (FALSE, WINDOW_ROW_SPACING);
  align_2 = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
  gtk_container_add (GTK_CONTAINER (align_2), box_2);
  gtk_box_pack_start (GTK_BOX (box), align_2, TRUE, TRUE, 0);

  w = g_new (SSWorkspace, 1);
  w->screen = screen;
  w->wnck_workspace = wnck_workspace;
  w->viewport = viewport;
  w->widget = align;
  w->header = header;
  w->window_container = box_2;
  w->title = "";
  w->windows = NULL;
  g_signal_connect (G_OBJECT (header), "expose-event",
    (GCallback) on_expose_event,
    w);
  g_signal_connect (G_OBJECT (header), "button-press-event",
    (GCallback) on_button_press_event,
    w);
  g_signal_connect (G_OBJECT (header), "button-release-event",
    (GCallback) on_button_release_event,
    w);
  g_signal_connect (G_OBJECT (header), "motion-notify-event",
    (GCallback) on_motion_notify_event,
    w);
  g_signal_connect (G_OBJECT (header), "scroll-event",
    (GCallback) on_scroll_event,
    w);
  g_object_ref (w->widget);
  return w;
}

//------------------------------------------------------------------------------

void
ss_workspace_free (SSWorkspace *workspace)
{
  if (workspace == NULL) {
    return;
  }
  g_list_free (workspace->windows);
  g_object_unref (workspace->widget);
  g_free (workspace);
}
