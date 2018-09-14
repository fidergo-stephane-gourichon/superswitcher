// Copyright (c) 2006 Nigel Tao.
// Licenced under the GNU General Public Licence (GPL) version 2.

#include "window.h"

#include "draganddrop.h"
#include "screen.h"
#include "workspace.h"
#include "xinerama.h"

#ifdef HAVE_XCOMPOSITE
#include "thumbnailer.h"
#endif

//------------------------------------------------------------------------------

void
ss_window_update_label_max_width_chars (SSWindow *window)
{
  gtk_label_set_max_width_chars (GTK_LABEL (window->label),
    window->screen->label_max_width_chars);
}

//------------------------------------------------------------------------------

static void
ss_window_set_bold (SSWindow *window, gboolean bold)
{
  PangoAttribute *pa;
  PangoAttrList *pal;

  pa = pango_attr_weight_new (bold ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL);
  pa->start_index = 0;
  pa->end_index = G_MAXINT;
  pal = pango_attr_list_new ();
  pango_attr_list_insert (pal, pa);
  gtk_label_set_attributes (GTK_LABEL (window->label), pal);
  pango_attr_list_unref (pal);
}

//------------------------------------------------------------------------------

static void
ss_window_set_italic (SSWindow *window, gboolean italic)
{
  PangoAttribute *pa;
  PangoAttrList *pal;

  pa = pango_attr_style_new (italic ? PANGO_STYLE_ITALIC : PANGO_STYLE_NORMAL);
  pa->start_index = 0;
  pa->end_index = G_MAXINT;
  pal = pango_attr_list_new ();
  pango_attr_list_insert (pal, pa);
  gtk_label_set_attributes (GTK_LABEL (window->label), pal);
  pango_attr_list_unref (pal);
}

//------------------------------------------------------------------------------

void
ss_window_set_selected (SSWindow *window, gboolean selected)
{
  gtk_widget_set_state (window->label,
    selected ? GTK_STATE_SELECTED : GTK_STATE_NORMAL);
}

//------------------------------------------------------------------------------

void
ss_window_set_sensitive (SSWindow *window, gboolean sensitive)
{
  gtk_widget_set_sensitive (GTK_WIDGET (window->image), sensitive);
  gtk_widget_set_sensitive (GTK_WIDGET (window->label), sensitive);
  window->sensitive = sensitive;
}

//------------------------------------------------------------------------------

void
ss_window_activate_workspace_and_window (SSWindow *window, guint32 time,
                                         gboolean also_warp_pointer_if_necessary)
{
  if (window == NULL) {
    return;
  }
  if (window->workspace != NULL) {
    if (window_manager_uses_viewports) {
      wnck_screen_move_viewport (
        window->screen->wnck_screen, window->screen->screen_width * window->workspace->viewport, 0);
    } else {
      wnck_workspace_activate (window->workspace->wnck_workspace, time);
    }
  }
  ss_window_activate_window (window, time + 1, also_warp_pointer_if_necessary);
}

//------------------------------------------------------------------------------

void
ss_window_activate_window (SSWindow *window, guint32 time,
                           gboolean also_warp_pointer_if_necessary)
{
  GdkRectangle r;
  if (window == NULL) {
    return;
  }

  wnck_window_activate (window->wnck_window, time);
  if (also_warp_pointer_if_necessary &&
      window->screen->pointer_needs_recentering_on_focus_change) {
    wnck_window_get_geometry (window->wnck_window, &r.x, &r.y, &r.width, &r.height);
    XWarpPointer (window->screen->xinerama->x_display,
                  None,
                  window->screen->xinerama->x_root_window,
                  0, 0, 0, 0, 
                  r.x + (r.width / 2), r.y + (r.height / 2));
  }
}

//------------------------------------------------------------------------------

void
ss_window_update_for_new_workspace (SSWindow *window, SSWorkspace *new_workspace)
{
  if (window->workspace == new_workspace) {
    return;
  }
  if (window->workspace) {
    ss_workspace_remove_window (window->workspace, window);
  }
  window->workspace = new_workspace;
  if (new_workspace) {
    ss_workspace_add_window (new_workspace, window);
  }
  window->new_window_index = -1;
  gtk_widget_queue_draw (gtk_widget_get_toplevel (window->widget));
}


//------------------------------------------------------------------------------

void
ss_window_move_to_workspace (SSWindow *window, SSWorkspace *workspace)
{
  int old_x, old_y, old_w, old_h;
  int frame_left, frame_right, frame_top, frame_bottom;
  int workspace_delta;
  int new_x;
  if (window->workspace == workspace) {
    return;
  }
  if (window->workspace == NULL) {
    // TODO - implement move for a floating (??) workspace-less window.
    return;
  }
  if (window_manager_uses_viewports) {
    wnck_window_get_geometry (window->wnck_window, &old_x, &old_y, &old_w, &old_h);
    workspace_delta = workspace->viewport - window->workspace->viewport;
    ss_xinerama_get_frame_extents (window->screen->xinerama, window,
      &frame_left, &frame_right, &frame_top, &frame_bottom);
    new_x = old_x + (workspace_delta * window->screen->screen_width) - frame_left;
    wnck_window_set_geometry (
      window->wnck_window, WNCK_WINDOW_GRAVITY_CURRENT, WNCK_WINDOW_CHANGE_X, new_x, 0, 0, 0);
  } else {
    wnck_window_move_to_workspace (window->wnck_window, workspace->wnck_workspace);
  }
}

//------------------------------------------------------------------------------

static gboolean
on_button_press_event (GtkWidget *widget, GdkEventButton *event, gpointer data)
{
  SSWindow *window;
  window = (SSWindow *) data;
  ss_draganddrop_start (window->screen->drag_and_drop, window, window->workspace);
  return TRUE;
}

//------------------------------------------------------------------------------

static gboolean
on_button_release_event (GtkWidget *widget, GdkEventButton *event, gpointer data)
{
  SSWindow *window;
  /* WnckWindow *wnck_window; */
  SSScreen *screen;
  SSDragAndDrop *dnd;
  WnckWorkspace *wnck_workspace;
  window = (SSWindow *) data;
  /* wnck_window = window->wnck_window; */
  screen = window->screen;
  dnd = screen->drag_and_drop;

  if (dnd->is_dragging) {
    if (dnd->drag_workspace != NULL) {
      wnck_workspace = dnd->drag_workspace->wnck_workspace;
      if (!window_manager_uses_viewports) {
        wnck_workspace_activate (wnck_workspace, event->time);
      }
      if (dnd->drag_start_workspace != dnd->drag_workspace) {
        window->new_window_index = dnd->new_window_index;
        ss_window_move_to_workspace (window, dnd->drag_workspace);
        if (window_manager_uses_viewports) {
          wnck_screen_move_viewport (
            screen->wnck_screen, screen->screen_width * dnd->drag_workspace->viewport, 0);
        }
      } else {
        // Make an adjustment because moving a window
        // to directly after itself should be a no-op,
        // just like moving a window to directly
        // before itself.  The former case is like
        // moving a window with g_list_index == 2
        // to a new position of new_window_index == 3,
        // which needs to be adjusted by -1.
        if (dnd->new_window_index >
          g_list_index (
          dnd->drag_start_workspace->windows,
          dnd->drag_start_window)) {

          dnd->new_window_index -= 1;
        }

        window->new_window_index = -1;
        ss_workspace_reorder_window (
          dnd->drag_start_workspace,
          dnd->drag_start_window,
          dnd->new_window_index);
      }
      ss_window_activate_window (window, event->time, FALSE);
    }
  } else {
    // It's a plain old click, not a drag.
    window->new_window_index = -1;
    ss_window_activate_workspace_and_window (window, event->time, FALSE);
  }
  ss_draganddrop_on_release (dnd);
  return TRUE;
}

//------------------------------------------------------------------------------

static gboolean
on_motion_notify_event (GtkWidget *widget, GdkEventMotion *event, gpointer data)
{
  ss_draganddrop_on_motion (((SSWindow *) data)->screen->drag_and_drop);
  return TRUE;
}

//------------------------------------------------------------------------------

static void
on_geometry_changed (WnckWindow *wnck_window, gpointer data)
{
  SSWindow *window;
  window = (SSWindow *) data;
  gtk_widget_queue_draw (gtk_widget_get_toplevel (window->widget));
  if (window_manager_uses_viewports) {
    ss_window_update_for_new_workspace (window,
      ss_screen_get_workspace_for_wnck_window (window->screen, wnck_window));
  }
}

//------------------------------------------------------------------------------

static void
on_icon_changed (WnckWindow *wnck_window, gpointer data)
{
  SSWindow *window;
  window = (SSWindow *) data;
  gtk_image_set_from_pixbuf (GTK_IMAGE(window->image),
    wnck_window_get_mini_icon (wnck_window));
}


//------------------------------------------------------------------------------

static void
on_name_changed (WnckWindow *wnck_window, gpointer data)
{
  SSWindow *window;
  const char *name;
  window = (SSWindow *) data;
  name = wnck_window_get_name (wnck_window);
  gtk_label_set_text (GTK_LABEL (window->label), name);
#ifdef HAVE_GTK_2_11
  gtk_widget_set_tooltip_text (window->widget, name);
#else
  if (window->workspace) {
    gtk_tooltips_set_tip (GTK_TOOLTIPS (window->screen->tooltips), window->widget, name, "");
  }
#endif
  gtk_widget_queue_draw (gtk_widget_get_toplevel (window->widget));
}

//------------------------------------------------------------------------------

static void
on_state_changed (WnckWindow *wnck_window, WnckWindowState changed_mask, WnckWindowState new_state, gpointer data)
{
  SSWindow *window;
  window = (SSWindow *) data;
#ifdef HAVE_WNCK_2_12
  if (changed_mask & (WNCK_WINDOW_STATE_DEMANDS_ATTENTION | WNCK_WINDOW_STATE_URGENT)) {
    ss_window_set_bold (window, wnck_window_needs_attention (wnck_window));
  }
#else
  if (changed_mask & (WNCK_WINDOW_STATE_DEMANDS_ATTENTION)) {
    ss_window_set_bold (window, wnck_window_demands_attention (wnck_window));
  }
#endif
  if (changed_mask & WNCK_WINDOW_STATE_MINIMIZED) {
    ss_window_set_italic (window, wnck_window_is_minimized (wnck_window));
  }
}

//------------------------------------------------------------------------------

static void
on_workspace_changed (WnckWindow *wnck_window, gpointer data)
{
  SSWindow *window;
  /* SSWorkspace *old_workspace; */
  SSWorkspace *new_workspace;
  WnckWorkspace *new_wnck_workspace;
  int new_workspace_id;

  window = (SSWindow *) data;
  /* old_workspace = window->workspace; */
  new_wnck_workspace = wnck_window_get_workspace (wnck_window);
  if (new_wnck_workspace) {
    new_workspace_id = wnck_workspace_get_number (new_wnck_workspace);
    new_workspace = ss_screen_get_nth_workspace (window->screen, new_workspace_id);
  } else {
    new_workspace_id = -1;
    new_workspace = NULL;
  }
  ss_window_update_for_new_workspace (window, new_workspace);
}

//------------------------------------------------------------------------------

static gboolean
on_expose_event (GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
  SSWindow *window;
  window = (SSWindow *) data;

  if (window == window->screen->active_window) {
    gtk_paint_box (widget->style,
      widget->window,
      GTK_STATE_NORMAL,
      GTK_SHADOW_NONE,
      NULL,
      widget,
      "menuitem",
      widget->allocation.x      - 2,
      widget->allocation.y      - 1,
      widget->allocation.width  + 4,
      widget->allocation.height + 2);
  }
  return FALSE;
}

//------------------------------------------------------------------------------

SSWindow *
ss_window_new (SSWorkspace *workspace, WnckWindow *wnck_window)
{
  SSWindow *w;
  GtkWidget *eventbox;
  GtkWidget *hbox;
  GtkWidget *image;
  GtkWidget *label;
  GdkColor *color;
#ifdef HAVE_XCOMPOSITE
  SSThumbnailer *thumbnailer;
#endif
  w = g_new (SSWindow, 1);

  eventbox = gtk_event_box_new ();
  gtk_event_box_set_visible_window (GTK_EVENT_BOX (eventbox), FALSE);
#ifdef HAVE_GTK_2_11
  gtk_widget_set_tooltip_text (eventbox, wnck_window_get_name (wnck_window));
#else
  gtk_tooltips_set_tip (GTK_TOOLTIPS (workspace->screen->tooltips),
    eventbox, wnck_window_get_name (wnck_window), "");
#endif

  hbox = gtk_hbox_new (FALSE, 3);
  gtk_container_add (GTK_CONTAINER (eventbox), hbox);

#ifdef HAVE_XCOMPOSITE
  if (show_window_thumbnails) {
    // Really, all I need is any GtkWidget (so that I can have an allocation
    // and set a preferred size) that doesn't have its own X window (so that
    // it doesn't try to paint itself).
    image = gtk_fixed_new ();
    thumbnailer = ss_thumbnailer_new (w, wnck_window, image);
  } else {
#endif
    image = gtk_image_new ();
    gtk_image_set_from_pixbuf (GTK_IMAGE(image), wnck_window_get_mini_icon (wnck_window));
#ifdef HAVE_XCOMPOSITE
    thumbnailer = NULL;
  }
#endif
  gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);

  label = gtk_label_new (wnck_window_get_name (wnck_window));
  gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
  gtk_label_set_max_width_chars (GTK_LABEL (label), workspace->screen->label_max_width_chars);
  gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_MIDDLE);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);

  color = & (gtk_widget_get_default_style ()->text[GTK_STATE_SELECTED]);
  gtk_widget_modify_fg (label, GTK_STATE_SELECTED, color);

  w->screen = workspace->screen;
  w->workspace = workspace;
  w->wnck_window = wnck_window;
  w->widget = eventbox;
  w->image = image;
  w->label = label;
#ifdef HAVE_XCOMPOSITE
  w->thumbnailer = thumbnailer;
#endif
  w->sensitive = TRUE;
  w->new_window_index = -1;
  w->signal_id_geometry_changed =
    g_signal_connect (G_OBJECT (wnck_window), "geometry-changed",
    (GCallback) on_geometry_changed,
    w);
  w->signal_id_icon_changed =
#ifdef HAVE_XCOMPOSITE
    show_window_thumbnails ? 0L :
#endif
    g_signal_connect (G_OBJECT (wnck_window), "icon-changed",
    (GCallback) on_icon_changed,
    w);
  w->signal_id_name_changed =
    g_signal_connect (G_OBJECT (wnck_window), "name-changed",
    (GCallback) on_name_changed,
    w);
  w->signal_id_state_changed =
    g_signal_connect (G_OBJECT (wnck_window), "state-changed",
    (GCallback) on_state_changed,
    w);
  w->signal_id_workspace_changed =
    g_signal_connect (G_OBJECT (wnck_window), "workspace-changed",
    (GCallback) on_workspace_changed,
    w);
  g_signal_connect (G_OBJECT (hbox), "expose-event",
    (GCallback) on_expose_event,
    w);
  g_signal_connect (G_OBJECT (eventbox), "button-press-event",
    (GCallback) on_button_press_event,
    w);
  g_signal_connect (G_OBJECT (eventbox), "button-release-event",
    (GCallback) on_button_release_event,
    w);
  g_signal_connect (G_OBJECT (eventbox), "motion-notify-event",
    (GCallback) on_motion_notify_event,
    w);
  g_object_ref (w->widget);
#ifdef HAVE_WNCK_2_12
  if (wnck_window_needs_attention (wnck_window)) {
    ss_window_set_bold (w, TRUE);
  }
#else
  if (wnck_window_demands_attention (wnck_window)) {
    ss_window_set_bold (w, TRUE);
  }
#endif
  return w;
}

//------------------------------------------------------------------------------

void
ss_window_free (SSWindow *window)
{
  if (window == NULL) {
    return;
  }
  g_signal_handler_disconnect (G_OBJECT (window->wnck_window),
    window->signal_id_geometry_changed);
  if (window->signal_id_icon_changed) {
    g_signal_handler_disconnect (G_OBJECT (window->wnck_window),
      window->signal_id_icon_changed);
  }
  g_signal_handler_disconnect (G_OBJECT (window->wnck_window),
    window->signal_id_name_changed);
  g_signal_handler_disconnect (G_OBJECT (window->wnck_window),
    window->signal_id_state_changed);
  g_signal_handler_disconnect (G_OBJECT (window->wnck_window),
    window->signal_id_workspace_changed);
  g_object_unref (window->widget);
#ifdef HAVE_XCOMPOSITE
  ss_thumbnailer_free (window->thumbnailer);
#endif
  g_free (window);
}
