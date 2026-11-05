// Copyright (c) 2006 Nigel Tao.
// Licenced under the GNU General Public Licence (GPL) version 2.

#include "workspace.h"

#include <X11/X.h>

#include "draganddrop.h"
#include "screen.h"
#include "style.h"
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
fill_rectangle (cairo_t *c, GdkRGBA *color, int x, int y, int width, int height)
{
  gdk_cairo_set_source_rgba (c, color);
  cairo_rectangle (c, x, y, width, height);
  cairo_fill (c);
}

//------------------------------------------------------------------------------

// The outline is one pixel wide and its outer size is width by height.
static void
outline_rectangle (cairo_t *c, GdkRGBA *color, int x, int y, int width, int height)
{
  gdk_cairo_set_source_rgba (c, color);
  cairo_set_line_width (c, 1.0);
  cairo_rectangle (c, x + 0.5, y + 0.5, width - 1, height - 1);
  cairo_stroke (c);
}

//------------------------------------------------------------------------------

static void
on_draw_text (GtkWidget *widget, cairo_t *c, SSWorkspace *workspace)
{
  cairo_text_extents_t extents;
  int x, y;

  cairo_select_font_face (c, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
  cairo_set_font_size (c, MINI_WORKSPACE_FONT_HEIGHT);

  cairo_text_extents (c, workspace->title, &extents);
  x = (gtk_widget_get_allocated_width (widget)  - extents.width)  / 2;
  y = (gtk_widget_get_allocated_height (widget) + extents.height) / 2;

  cairo_set_source_rgba (c, 0, 0, 0, 0.25);
  cairo_move_to (c, x-1, y);
  cairo_show_text (c, workspace->title);
  cairo_set_source_rgba (c, 1, 1, 1, 0.25);
  cairo_move_to (c, x, y+1);
  cairo_show_text (c, workspace->title);
}

//------------------------------------------------------------------------------

static gboolean
on_draw (GtkWidget *widget, cairo_t *c, gpointer data)
{
  SSWorkspace *workspace;
  int screen_width, screen_height;
  double width_ratio, height_ratio;
  int w, h;
  int viewport_x;
  GList *i;
  SSWindow *active_window;
  WnckWindow *wnck_window;
  gboolean is_active;
  GdkRGBA color;
  GdkRectangle r;

  workspace = (SSWorkspace *) data;
  screen_width  = workspace->screen->screen_width;
  screen_height = workspace->screen->screen_height;
  active_window = workspace->screen->active_window;

  w = gtk_widget_get_allocated_width (widget);
  h = gtk_widget_get_allocated_height (widget);

  is_active = (workspace == workspace->screen->active_workspace);
  ss_style_get_color (widget, is_active ? SS_COLOR_ACTIVE_WORKSPACE : SS_COLOR_WORKSPACE, &color);
  fill_rectangle (c, &color, 1, 1, w-2, h-2);
  ss_style_get_color (widget, is_active ? SS_COLOR_ACTIVE_WORKSPACE_BORDER : SS_COLOR_WORKSPACE_BORDER, &color);
  outline_rectangle (c, &color, 0, 0, w, h);

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

    is_active = (active_window != NULL) &&
      (wnck_window_get_xid (wnck_window) == wnck_window_get_xid (active_window->wnck_window));
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

    ss_style_get_color (widget, is_active ? SS_COLOR_ACTIVE_WINDOW : SS_COLOR_WINDOW, &color);
    fill_rectangle (c, &color, r.x+1, r.y+1, r.width-2, r.height-2);
    ss_style_get_color (widget, is_active ? SS_COLOR_ACTIVE_WINDOW_BORDER : SS_COLOR_WINDOW_BORDER, &color);
    outline_rectangle (c, &color, r.x, r.y, r.width, r.height);
  }

  on_draw_text (widget, c, workspace);
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
  GtkAllocation a;

  n = g_list_length (workspace->windows);
  if (n == 0) {
    return -1;
  }

  for (i = workspace->windows, index = 0; i; i = i->next, index++) {
    window = (SSWindow *) i->data;
    gtk_widget_get_allocation (window->widget, &a);
    if (y < (a.y + (a.height + WINDOW_ROW_SPACING) / 2)) {
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
  GtkWidget *column;
  GtkWidget *box;
  GtkWidget *header;
  GtkWidget *box_2;

  // The column has the full height of the popup, and the drag outline of a
  // workspace uses that height.  The box has the height of its content.
  column = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 3);
  gtk_widget_set_halign (box, GTK_ALIGN_CENTER);
  gtk_box_pack_start (GTK_BOX (column), box, FALSE, FALSE, 0);

  header = gtk_drawing_area_new ();
  // GDK_SCROLL_MASK is needed because, unlike GTK 2, GTK 3 does not deliver
  // scroll events to a widget that only selects button presses.  Motion hints
  // are not used, because with GTK 3 they stop the motion events until a call
  // to gdk_event_request_motions.
  gtk_widget_add_events (header,
                         GDK_BUTTON_PRESS_MASK |
                         GDK_BUTTON_RELEASE_MASK |
                         GDK_POINTER_MOTION_MASK |
                         GDK_SCROLL_MASK);
  gtk_widget_set_size_request (header, MINI_WORKSPACE_WIDTH,
    MINI_WORKSPACE_WIDTH * screen->screen_aspect);

  gtk_widget_set_halign (header, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (header, GTK_ALIGN_CENTER);
  gtk_box_pack_start (GTK_BOX (box), header, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (box), gtk_separator_new (GTK_ORIENTATION_HORIZONTAL), TRUE, TRUE, 0);

  box_2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, WINDOW_ROW_SPACING);
  gtk_widget_set_halign (box_2, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (box_2, GTK_ALIGN_CENTER);
  gtk_box_pack_start (GTK_BOX (box), box_2, TRUE, TRUE, 0);

  w = g_new (SSWorkspace, 1);
  w->screen = screen;
  w->wnck_workspace = wnck_workspace;
  w->viewport = viewport;
  w->widget = column;
  w->header = header;
  w->window_container = box_2;
  w->title = "";
  w->windows = NULL;
  g_signal_connect (G_OBJECT (header), "draw",
    (GCallback) on_draw,
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
