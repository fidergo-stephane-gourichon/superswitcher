// Copyright (c) 2006 Nigel Tao.
// Licenced under the GNU General Public Licence (GPL) version 2.

#include "popup.h"

#include <gdk/gdkx.h>
#include <libwnck/libwnck.h>
#include <X11/keysym.h>
#include <X11/X.h>
#include "string.h"

#include "draganddrop.h"
#include "window.h"
#include "workspace.h"
#include "xinerama.h"

//------------------------------------------------------------------------------

static void
action_change_active_window_by_delta (Popup *popup, int delta, gboolean also_bring_active_window,
  guint32 time, gboolean also_warp_pointer_if_necessary)
{
  GList *window_list;
  GList *i;
  SSWindow *aw;
  SSWindow *window;
  SSWindow *previous_window;
  gboolean should_activate_next_window;
  int n, num_windows;

  if (popup->screen->active_workspace == NULL) {
    return;
  }

  window_list = popup->screen->active_workspace->windows;
  aw = popup->screen->active_window;
  num_windows = g_list_length (window_list);
  if (aw == NULL) {
    if (num_windows > 0) {
      if (delta == +1) {
        window = (SSWindow *) (g_list_first (window_list))->data;
      } else {
        window = (SSWindow *) (g_list_last  (window_list))->data;
      }
      ss_window_activate_window (window, time, also_warp_pointer_if_necessary);
    }
    return;
  }

  if (num_windows <= 1) {
    return;
  }

  if (also_bring_active_window) {
    n = g_list_index (window_list, aw) + delta;
    if (n == -1) {
      n = num_windows - 1;
    }
    else if (n == num_windows) {
      n = 0;
    }
    ss_workspace_reorder_window (popup->screen->active_workspace, aw, n);
    gtk_widget_queue_draw (popup->window);
    return;
  }

  should_activate_next_window = FALSE;

  previous_window = (SSWindow *) (g_list_last (window_list))->data;
  for (i = window_list; i; i = i->next) {
    window = (SSWindow *) i->data;

    if (should_activate_next_window) {
      ss_window_activate_window (window, time, also_warp_pointer_if_necessary);
      return;
    }

    if (window == aw) {
      if (delta == +1) {
        should_activate_next_window = TRUE;
      } else if (delta == -1) {
        ss_window_activate_window (previous_window, time, also_warp_pointer_if_necessary);
        return;
      } else {
        g_assert_not_reached ();
      }
    }

    previous_window = window;
  }

  if (should_activate_next_window) {
    window = (SSWindow *) (g_list_first (window_list))->data;
    ss_window_activate_window (window, time, also_warp_pointer_if_necessary);
  }
}

//------------------------------------------------------------------------------

static void
action_change_active_window_by_stacking_order (Popup *popup, gboolean backwards, guint32 time)
{
  ss_screen_activate_next_window_in_stacking_order (popup->screen, backwards, time);
}

//------------------------------------------------------------------------------

static void
action_change_active_workspace_by_delta (Popup *popup, int delta, gboolean also_bring_active_window, gboolean all_not_just_current_window, guint32 time)
{
  ss_screen_change_active_workspace_by_delta (popup->screen, delta,
    also_bring_active_window, all_not_just_current_window, time);
}

//------------------------------------------------------------------------------

static void
action_change_active_workspace (Popup *popup, int n, gboolean also_bring_active_window, gboolean all_not_just_current_window, guint32 time)
{
  ss_screen_change_active_workspace (popup->screen, n,
    also_bring_active_window, all_not_just_current_window, time);
}

//------------------------------------------------------------------------------

static void
action_new_workspace (Popup *popup, gboolean also_bring_active_window, gboolean all_not_just_current_window, guint32 time)
{
  if (popup->screen->num_workspaces >= MAX_REASONABLE_WORKSPACES) {
    return;
  }

  // TODO - be compiz-aware
  wnck_screen_change_workspace_count (popup->screen->wnck_screen,
    popup->screen->num_workspaces + 1);

  // For some reason, libwnck does not reflect the change right away -
  // wnck_screen_get_workspace_count returns the old count, not the
  // new count.  Accordingly, we have to activate the new workspace in
  // the signal callback (on_workspace_created), not here.
  popup->owc_complete_action_new_workspace = TRUE;
  popup->owc_also_bring_active_window = also_bring_active_window;
  popup->owc_all_not_just_current_window = all_not_just_current_window;
  popup->owc_time = time;
}

//------------------------------------------------------------------------------

static void
action_delete_workspace_if_empty (Popup *popup, gboolean all_not_just_current_workspace, guint32 time)
{
  // libwnck does not have a delete-specific-workspace function - only
  // a change_workspace_count function.  Thus, we have this little hack
  // where we move a whole bunch of windows one or more workspaces to the
  // left before changing the count, to give the *appearance* of deleting
  // a specific workspace.
  GList *workspaces;

  SSWorkspace *workspace;
  SSWindow *window;
  GList *i;
  GList *j;

  int num_workspaces_deleted;
  gboolean active_workspace_has_been_seen;

  GList *wnck_windows_to_move;
  GList *wnck_workspaces_to_move_to;

  GList *wwtmt_as_glist;
  WnckWorkspace *wnck_workspace_to_move_to;

  WnckWorkspace *wnck_workspace_to_activate;


  // Initialization
  workspaces = popup->screen->workspaces;

  num_workspaces_deleted = 0;
  wnck_windows_to_move = NULL;
  wnck_workspaces_to_move_to = NULL;

  wnck_workspace_to_activate = NULL;


  // We proceed in two stages.  First, we make two parallel lists of
  // the wnck_windows we want to move (and the wnck_workspaces we will
  // move them to).  Once we have constructed these lists in their
  // entirety, we will do the actual moving.
  // Doing this in two stages, rather than moving windows as we find them,
  // avoids (indirectly) changing the underlying data structures whilst
  // we're trying to compute what to move.
  if (all_not_just_current_workspace) {
    // Delete all empty workspaces

    wwtmt_as_glist = workspaces;
    wnck_workspace_to_move_to = ((SSWorkspace *) wwtmt_as_glist->data)->wnck_workspace;

    for (i = workspaces; i; i = i->next) {
      workspace = (SSWorkspace *) i->data;

      if (workspace == popup->screen->active_workspace) {
        wnck_workspace_to_activate = wnck_workspace_to_move_to;
      }

      if (g_list_length (workspace->windows) > 0) {
        for (j = workspace->windows; j; j = j->next) {
          window = (SSWindow *) j->data;
          wnck_windows_to_move = g_list_append
            (wnck_windows_to_move, window->wnck_window);
          wnck_workspaces_to_move_to = g_list_append
            (wnck_workspaces_to_move_to, wnck_workspace_to_move_to);
        }

        wwtmt_as_glist = wwtmt_as_glist->next;
        wnck_workspace_to_move_to = ((SSWorkspace *) wwtmt_as_glist->data)->wnck_workspace;
      } else {
        num_workspaces_deleted++;
      }
    }


  } else {
    // Delete only the active workspace, and only if it is empty.

    if (popup->screen->active_workspace != NULL &&
        g_list_length (popup->screen->active_workspace->windows) == 0) {
      active_workspace_has_been_seen = FALSE;
      wnck_workspace_to_move_to = NULL;
      for (i = workspaces; i; i = i->next) {
        workspace = (SSWorkspace *) i->data;

        if (workspace == popup->screen->active_workspace) {
          active_workspace_has_been_seen = TRUE;
        }
        else if (active_workspace_has_been_seen) {
          for (j = workspace->windows; j; j = j->next) {
            window = (SSWindow *) j->data;
            wnck_windows_to_move = g_list_append
              (wnck_windows_to_move, window->wnck_window);
            wnck_workspaces_to_move_to = g_list_append
              (wnck_workspaces_to_move_to, wnck_workspace_to_move_to);
          }
        }

        wnck_workspace_to_move_to = workspace->wnck_workspace;
      }
      num_workspaces_deleted++;
    }
  }

  // TODO - fix the moves / activations when window_manager_uses_viewports

  // Second stage - do the actual moves.
  // 2a) move the windows.
  for (i = wnck_windows_to_move, j = wnck_workspaces_to_move_to; i; i = i->next, j = j->next) {
    wnck_window_move_to_workspace ((WnckWindow *) i->data,
      (WnckWorkspace *) j->data);
  }
  g_list_free (wnck_windows_to_move);
  g_list_free (wnck_workspaces_to_move_to);

  // 2b) maintain what appears to be the active workspace, if we're
  // deleting workspaces in bulk.
  if (wnck_workspace_to_activate != NULL) {
    wnck_workspace_activate (wnck_workspace_to_activate, time);
  }

  // 2c) remove the empty workspaces at the end.
  if (num_workspaces_deleted > 0) {
    wnck_screen_change_workspace_count (popup->screen->wnck_screen,
      MAX(1, popup->screen->num_workspaces - num_workspaces_deleted));
  }

  // The highlight for the active window may need to be re-drawn.
  gtk_widget_queue_draw (popup->window);
}

//------------------------------------------------------------------------------

static void
action_close_active_window (Popup *popup, gboolean all_windows_in_workspace, guint32 time)
{
  SSWorkspace *workspace;
  SSWindow *window;
  GList *i;

  if (all_windows_in_workspace) {
    workspace = popup->screen->active_workspace;
    if (workspace == NULL) {
      return;
    }

    for (i = workspace->windows; i; i = i->next) {
      window = (SSWindow *) i->data;
      wnck_window_close (window->wnck_window, time);
    }

  } else {
    window = popup->screen->active_window;
    if (window == NULL) {
      return;
    }

    wnck_window_close (window->wnck_window, time);
  }
}

//------------------------------------------------------------------------------

static void
action_window_toggle_maximize (Popup *popup, gboolean all_windows_in_workspace, guint32 time)
{
  SSWorkspace *workspace;
  SSWindow *window;
  gboolean all_windows_are_maximized;
  GList *i;

  if (all_windows_in_workspace) {
    workspace = popup->screen->active_workspace;
    if (workspace == NULL) {
      return;
    }

    all_windows_are_maximized = TRUE;
    for (i = workspace->windows; i; i = i->next) {
      window = (SSWindow *) i->data;
      if (! wnck_window_is_maximized (window->wnck_window)) {
        all_windows_are_maximized = FALSE;
        break;
      }
    }

    if (all_windows_are_maximized) {
      for (i = workspace->windows; i; i = i->next) {
        window = (SSWindow *) i->data;
        wnck_window_unmaximize (window->wnck_window);
      }
    } else {
      for (i = workspace->windows; i; i = i->next) {
        window = (SSWindow *) i->data;
        wnck_window_maximize (window->wnck_window);
      }
    }

  } else {
    window = popup->screen->active_window;
    if (window == NULL) {
      return;
    }

    if (wnck_window_is_maximized (window->wnck_window)) {
      wnck_window_unmaximize (window->wnck_window);
    } else {
      wnck_window_maximize (window->wnck_window);
    }
  }
}

//------------------------------------------------------------------------------

static void
action_window_toggle_minimize (Popup *popup, gboolean all_windows_in_workspace, guint32 time)
{
  SSWorkspace *workspace;
  SSWindow *window;
  gboolean all_windows_are_minimized;
  GList *i;

  if (all_windows_in_workspace) {
    workspace = popup->screen->active_workspace;
    if (workspace == NULL) {
      return;
    }

    all_windows_are_minimized = TRUE;
    for (i = workspace->windows; i; i = i->next) {
      window = (SSWindow *) i->data;
      if (! wnck_window_is_minimized (window->wnck_window)) {
        all_windows_are_minimized = FALSE;
        break;
      }
    }

    if (all_windows_are_minimized) {
      for (i = workspace->windows; i; i = i->next) {
        window = (SSWindow *) i->data;
        wnck_window_unminimize (window->wnck_window, time);
      }
    } else {
      for (i = workspace->windows; i; i = i->next) {
        window = (SSWindow *) i->data;
        wnck_window_minimize (window->wnck_window);
      }
    }

  } else {
    window = popup->screen->active_window;
    if (window == NULL) {
      return;
    }

    if (wnck_window_is_minimized (window->wnck_window)) {
      wnck_window_unminimize (window->wnck_window, time);
    } else {
      wnck_window_minimize (window->wnck_window);
    }
  }
}

//------------------------------------------------------------------------------

static void
action_activate_next_window (Popup *popup, gboolean backwards, guint32 time)
{
  ss_screen_activate_next_window (popup->screen, backwards, time);
}

//------------------------------------------------------------------------------

static void
action_change_xinerama (Popup *popup, guint32 time)
{
  ss_xinerama_move_to_next_screen (popup->screen->xinerama, popup->screen->active_window);
}

//------------------------------------------------------------------------------

static void
on_active_window_changed (SSScreen *screen, gpointer data)
{
  Popup *popup;
  popup = (Popup *) data;
  gtk_widget_queue_draw (popup->window);
}

//------------------------------------------------------------------------------

static void
on_active_workspace_changed (SSScreen *screen, gpointer data)
{
  Popup *popup;
  popup = (Popup *) data;
  gtk_widget_queue_draw (popup->window);
}

//------------------------------------------------------------------------------

static void
on_window_opened (SSScreen *screen, SSWindow *window, gpointer data)
{
  if (window == NULL) {
    return;
  }
  gtk_widget_show_all (window->widget);
}

//------------------------------------------------------------------------------

static void
on_window_closed (SSScreen *screen, SSWindow *window, gpointer data)
{
  // No-op.
}

//------------------------------------------------------------------------------

static void
on_workspace_created (SSScreen *screen, SSWorkspace *workspace, gpointer data)
{
  GList *i;
  SSWindow *window;
  Popup *popup;
  popup = (Popup *) data;

  // This part below is a (possibly race-condition prone) hack - see
  // action_new_workspace for the reason.
  if (popup->owc_complete_action_new_workspace) {
    if (popup->owc_also_bring_active_window) {
      if (popup->owc_all_not_just_current_window) {
        if (screen->active_workspace != NULL) {
          for (i = screen->active_workspace->windows; i; i = i->next) {
            window = (SSWindow *) i->data;
            wnck_window_move_to_workspace (
              window->wnck_window,
              workspace->wnck_workspace);
          }
        }  // end if (screen->active_workspace != NULL)
      } else {  // else if (popup->owc_all_not_just_current_window)
        if (screen->active_window != NULL) {
          wnck_window_move_to_workspace (
            screen->active_window->wnck_window,
            workspace->wnck_workspace);
        }
      }  // end if (popup->owc_all_not_just_current_window)
    }  // end if (popup->owc_also_bring_active_window)

    wnck_workspace_activate (workspace->wnck_workspace, popup->owc_time);

    popup->owc_complete_action_new_workspace = FALSE;
    popup->owc_also_bring_active_window = FALSE;
    popup->owc_all_not_just_current_window = FALSE;
    popup->owc_time = -1;
  }
}

//------------------------------------------------------------------------------

static void
on_workspace_destroyed (SSScreen *screen, SSWorkspace *workspace, gpointer data)
{
  // no-op
}

//------------------------------------------------------------------------------

static gboolean
on_scroll_event (GtkWidget *widget, GdkEventScroll *event, gpointer data)
{
  Popup *popup;
  gboolean shifted;

  popup = (Popup *) data;
  shifted = ((event->state & ShiftMask) == ShiftMask);

  switch (event->direction) {
  case GDK_SCROLL_UP:
  case GDK_SCROLL_LEFT:
    action_change_active_window_by_delta (popup, -1, shifted, event->time, FALSE);
    break;

  case GDK_SCROLL_DOWN:
  case GDK_SCROLL_RIGHT:
    action_change_active_window_by_delta (popup, +1, shifted, event->time, FALSE);
    break;

  default:
    g_assert_not_reached ();
  }
  return TRUE;
}

//------------------------------------------------------------------------------

static gboolean
on_leave_notify_event (GtkWidget *widget, GdkEvent *event, gpointer data)
{
  if (event->type == GDK_LEAVE_NOTIFY &&
      event->crossing.detail != GDK_NOTIFY_INFERIOR) {
    // TODO - hide the popup?  The line below crashes.  I should fix this.
    // ss_dbus_hide_popup (NULL, NULL);
  }

  return TRUE;
}

//------------------------------------------------------------------------------

static gboolean
on_expose_event (GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
#ifdef HAVE_GTK_2_8
  Popup *popup;
  SSScreen *screen;
  SSDragAndDrop *dnd;
  cairo_t *c;
  GdkColormap *colormap;
  GdkGC *gc;
  GdkGCValues gc_values;
  GdkColor color;
  int rr, gg, bb;
  GtkAllocation *a;
  int oldx0, oldx1, oldy0, oldy1, oldhalfheight;
  int newx0, newx1, newy0, newy1;
  int n;
  int y;
  popup = (Popup *) data;
  screen = popup->screen;

  dnd = screen->drag_and_drop;
  if ((dnd->is_dragging) && (dnd->drag_workspace != NULL)) {
    c = gdk_cairo_create (popup->window->window);
    cairo_set_line_width (c, 2.0);

    // Some contortions just to get the theme color as 0-255 RGB.
    colormap = gdk_colormap_get_system ();
    gc = widget->style->text_gc[GTK_STATE_NORMAL];
    gdk_gc_get_values (gc, &gc_values);
    gdk_colormap_query_color (colormap, gc_values.foreground.pixel, &color);
    rr = (255 * color.red)   / 65535;
    gg = (255 * color.green) / 65535;
    bb = (255 * color.blue)  / 65535;

    if (dnd->drag_start_window != NULL) {
      a = &(dnd->drag_start_window->widget->allocation);
      oldx0 = -3 + a->x;
      oldx1 = +3 + a->x + a->width;
      oldy0 = -1 + a->y;
      oldy1 = +1 + a->y + a->height;
      oldhalfheight = a->height / 2;

      a = &(dnd->drag_workspace->widget->allocation);
      newx0 = -3 + a->x;
      newx1 = +3 + a->x + a->width;
      a = &(dnd->drag_workspace->window_container->allocation);
      y = a->y;
      n = g_list_length (dnd->drag_workspace->windows);
      if (n != 0) {
        if (dnd->new_window_index != -1) {
          y += (a->height * dnd->new_window_index) / n;
        }
      } else {
        y += oldhalfheight;
      }
      newy0 = -1 + y - oldhalfheight;
      newy1 = +1 + y + oldhalfheight;
    } else {
      a = &(dnd->drag_start_workspace->widget->allocation);
      oldx0 = -4 + a->x;
      oldx1 = +4 + a->x + a->width;
      oldy0 = -2 + a->y;
      oldy1 = +2 + a->y + a->height;

      a = &(dnd->drag_workspace->widget->allocation);
      newx0 = -4 + a->x;
      newx1 = +4 + a->x + a->width;
      newy0 = -2 + a->y;
      newy1 = +2 + a->y + a->height;
    }

    cairo_move_to (c, oldx0, oldy0);
    cairo_line_to (c, oldx1, oldy0);
    cairo_line_to (c, oldx1, oldy1);
    cairo_line_to (c, oldx0, oldy1);
    cairo_close_path (c);
    cairo_set_source_rgba (c, rr, gg, bb, 0.25);
    cairo_stroke (c);

    cairo_move_to (c, newx0, newy0);
    cairo_line_to (c, newx1, newy0);
    cairo_line_to (c, newx1, newy1);
    cairo_line_to (c, newx0, newy1);
    cairo_close_path (c);
    cairo_set_source_rgba (c, rr, gg, bb, 0.75);
    cairo_stroke (c);
    cairo_destroy (c);
  }
#endif
  return FALSE;
}

//------------------------------------------------------------------------------

static void
search_widget_create (Popup *popup)
{
  GtkWidget *box;
  PangoAttribute *pa;
  PangoAttrList *pal;

  box = gtk_hbox_new (FALSE, WORKSPACE_COLUMN_SPACING);
  gtk_container_add (GTK_CONTAINER (popup->search_container), box);

  gtk_box_pack_start (GTK_BOX (box), gtk_label_new ("Find:"), FALSE, FALSE, 0);

  popup->search_text_label = gtk_label_new (NULL);
  pa = pango_attr_weight_new (PANGO_WEIGHT_BOLD);
  pa->start_index = 0;
  pa->end_index = G_MAXINT;
  pal = pango_attr_list_new ();
  pango_attr_list_insert (pal, pa);
  gtk_label_set_attributes (GTK_LABEL (popup->search_text_label), pal);
  pango_attr_list_unref (pal);
  gtk_box_pack_start (GTK_BOX (box), popup->search_text_label, FALSE, FALSE, 0);

  popup->search_num_matches_label = gtk_label_new (NULL);
  gtk_box_pack_start (GTK_BOX (box), popup->search_num_matches_label, FALSE, FALSE, 0);

  gtk_widget_show_all (box);
}

//------------------------------------------------------------------------------

static GtkWidget *
button_create (Popup *popup, const gchar *stock_id, GCallback callback, const gchar *tooltip)
{
  GtkWidget *button;
  GtkWidget *image;

  button = gtk_button_new ();
  gtk_button_set_focus_on_click (GTK_BUTTON (button), FALSE);
  gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
  image = gtk_image_new_from_stock (stock_id, GTK_ICON_SIZE_MENU);
  gtk_container_add (GTK_CONTAINER (button), image);
#ifdef HAVE_GTK_2_11
  gtk_widget_set_tooltip_text (GTK_WIDGET (button), tooltip);
#else
  gtk_tooltips_set_tip (GTK_TOOLTIPS (popup->screen->tooltips),
    button, tooltip, "");
#endif
  g_signal_connect (G_OBJECT (button), "clicked", callback, popup);

  return button;
}

//------------------------------------------------------------------------------

static void
on_insert_button_clicked (GtkWidget *button, gpointer data)
{
  Popup *popup;
  GdkEvent *current_event;
  gboolean shifted;
  gboolean ctrled;
  guint32 time;

  popup = (Popup *) data;
  current_event = gtk_get_current_event ();
  if (current_event) {
    if (current_event->type == GDK_BUTTON_RELEASE) {
      shifted = ((current_event->button.state & ShiftMask) == ShiftMask);
      ctrled  = ((current_event->button.state & ControlMask) == ControlMask);
      time = current_event->button.time;
      action_new_workspace (popup, shifted, ctrled, time);
    }
    gdk_event_free (current_event);
  }
}

//------------------------------------------------------------------------------

static void
on_delete_button_clicked (GtkWidget *button, gpointer data)
{
  Popup *popup;
  GdkEvent *current_event;
  gboolean shifted;
  gboolean ctrled;
  guint32 time;

  popup = (Popup *) data;
  current_event = gtk_get_current_event ();
  if (current_event) {
    if (current_event->type == GDK_BUTTON_RELEASE) {
      shifted = ((current_event->button.state & ShiftMask) == ShiftMask);
      ctrled  = ((current_event->button.state & ControlMask) == ControlMask);
      time = current_event->button.time;
      action_delete_workspace_if_empty (popup, shifted | ctrled, time);
    }
    gdk_event_free (current_event);
  }
}

//------------------------------------------------------------------------------

static void
button_bar_create (Popup *popup, GtkWidget *parent_box)
{
  GtkWidget *inner_box;
  GtkWidget *align;
  GtkWidget *insert_button;
  GtkWidget *delete_button;

  inner_box = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (parent_box), inner_box, FALSE, TRUE, 0);

  insert_button = button_create (popup, GTK_STOCK_ADD,
                                 G_CALLBACK (on_insert_button_clicked),
                                 "Add a new workspace");
  gtk_box_pack_start (GTK_BOX (inner_box), insert_button, FALSE, FALSE, 0);

  delete_button = button_create (popup, GTK_STOCK_REMOVE,
                                 G_CALLBACK (on_delete_button_clicked),
                                 "Remove this workspace (if empty)");
  gtk_box_pack_start (GTK_BOX (inner_box), delete_button, FALSE, FALSE, 0);

  align = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
  popup->search_container = align;
  gtk_box_pack_start (GTK_BOX (inner_box), align, TRUE, TRUE, 0);
}

//------------------------------------------------------------------------------

static void
update_search (Popup *popup)
{
  int n;
  char *s;
  const char* query;

  query = gtk_label_get_text (GTK_LABEL (popup->search_text_label));
  ss_screen_update_search (popup->screen, query);
  n = popup->screen->num_search_matches;

  s = (n == 1)
    ? "(1 match)"
    : g_strdup_printf ("(%d matches)", n);
  gtk_label_set_text (GTK_LABEL (popup->search_num_matches_label), s);
  if (n != 1) {
    g_free (s);
  }
}

//------------------------------------------------------------------------------

Popup *
popup_create (SSScreen *screen)
{
  Popup *popup;
  GtkWidget *frame;
  GtkWidget *vbox;
  GtkWidget *align;

  update_window_label_width(screen);

  ss_screen_update_search (screen, "");

  popup = g_new (Popup, 1);
  popup->screen = screen;

  popup->search_text_label = NULL;
  popup->search_num_matches_label = NULL;

  popup->owc_complete_action_new_workspace = FALSE;
  popup->owc_also_bring_active_window = FALSE;
  popup->owc_all_not_just_current_window = FALSE;
  popup->owc_time = -1;

  popup->signal_id_active_window_changed =
    g_signal_connect (G_OBJECT (screen), "active-window-changed",
    (GCallback) on_active_window_changed,
    popup);
  popup->signal_id_active_workspace_changed =
    g_signal_connect (G_OBJECT (screen), "active-workspace-changed",
    (GCallback) on_active_workspace_changed,
    popup);
  popup->signal_id_window_closed =
    g_signal_connect (G_OBJECT (screen), "window-closed",
    (GCallback) on_window_closed,
    popup);
  popup->signal_id_window_opened =
    g_signal_connect (G_OBJECT (screen), "window-opened",
    (GCallback) on_window_opened,
    popup);
  popup->signal_id_workspace_created =
    g_signal_connect (G_OBJECT (screen), "workspace-created",
    (GCallback) on_workspace_created,
    popup);
  popup->signal_id_workspace_destroyed =
    g_signal_connect (G_OBJECT (screen), "workspace-destroyed",
    (GCallback) on_workspace_destroyed,
    popup);

  popup->window = gtk_window_new (GTK_WINDOW_POPUP);
  gtk_window_set_position (GTK_WINDOW (popup->window), GTK_WIN_POS_CENTER_ALWAYS);
  gtk_widget_add_events (popup->window, GDK_SCROLL_MASK | GDK_LEAVE_NOTIFY_MASK);
  g_signal_connect (G_OBJECT (popup->window), "scroll-event",
    (GCallback) on_scroll_event,
    popup);
  g_signal_connect (G_OBJECT (popup->window), "leave-notify-event",
    (GCallback) on_leave_notify_event,
    popup);
  g_signal_connect_after (G_OBJECT (popup->window), "expose-event",
    (GCallback) on_expose_event,
    popup);

  vbox = gtk_vbox_new (FALSE, 2);

  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_OUT);
  gtk_container_add (GTK_CONTAINER (popup->window), frame);
  gtk_container_add (GTK_CONTAINER (frame), vbox);

  align = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
  gtk_box_pack_start (GTK_BOX (vbox), align, TRUE, TRUE, 0);
  gtk_container_add (GTK_CONTAINER (align), screen->widget);
  popup->screen_container = align;

  gtk_box_pack_start (GTK_BOX (vbox), gtk_hseparator_new (), FALSE, FALSE, 0);
  button_bar_create (popup, vbox);

  gtk_widget_show_all (popup->window);
  return popup;
}

//------------------------------------------------------------------------------

void
popup_free (Popup *popup)
{
  ss_screen_update_wnck_windows_in_stacking_order (popup->screen);
  gtk_container_remove (GTK_CONTAINER (popup->screen_container),
    popup->screen->widget);

  g_signal_handler_disconnect (G_OBJECT (popup->screen),
    popup->signal_id_active_window_changed);
  g_signal_handler_disconnect (G_OBJECT (popup->screen),
    popup->signal_id_active_workspace_changed);
  g_signal_handler_disconnect (G_OBJECT (popup->screen),
    popup->signal_id_window_closed);
  g_signal_handler_disconnect (G_OBJECT (popup->screen),
    popup->signal_id_window_opened);
  g_signal_handler_disconnect (G_OBJECT (popup->screen),
    popup->signal_id_workspace_created);
  g_signal_handler_disconnect (G_OBJECT (popup->screen),
    popup->signal_id_workspace_destroyed);

  gtk_widget_destroy (popup->window);
  g_free (popup);
}

//------------------------------------------------------------------------------

void
popup_on_key_press (Popup *popup, Display *x_display, XKeyEvent *x_key_event)
{
  char key_string[4];
  KeySym keysym;
  gboolean shifted;
  gboolean ctrled;
  guint32 time;
  int n;
  const gchar *old_search_text;
  gchar *new_search_text;

  key_string[0] = '\0';
  key_string[1] = '\0';
  key_string[2] = '\0';
  key_string[3] = '\0';
  XLookupString (x_key_event, key_string, 3, NULL, NULL);

  keysym = XKeycodeToKeysym (x_display, x_key_event->keycode, 0);
  shifted = ((x_key_event->state & ShiftMask) == ShiftMask);
  ctrled  = ((x_key_event->state & ControlMask) == ControlMask);
  time = x_key_event->time;

  if ((keysym == XK_Left) || (keysym == XK_KP_Left)) {
    action_change_active_workspace_by_delta (popup, -1, shifted, ctrled, time);
  }
  else if ((keysym == XK_Right) || (keysym == XK_KP_Right)) {
    action_change_active_workspace_by_delta (popup, +1, shifted, ctrled, time);
  }
  else if ((keysym == XK_Up) || (keysym == XK_KP_Up)) {
    action_change_active_window_by_delta (popup, -1, shifted, time, TRUE);
  }
  else if ((keysym == XK_Down) || (keysym == XK_KP_Down)) {
    action_change_active_window_by_delta (popup, +1, shifted, time, TRUE);
  }
  else if ((keysym == XK_Page_Up) || (keysym == XK_KP_Page_Up)) {
    action_window_toggle_maximize (popup, ctrled, time);
  }
  else if ((keysym == XK_Page_Down) || (keysym == XK_KP_Page_Down)) {
    action_window_toggle_minimize (popup, ctrled, time);
  }
  else if ((keysym == XK_Insert) || (keysym == XK_KP_Insert)) {
    action_new_workspace (popup, shifted, ctrled, time);
  }
  else if ((keysym == XK_Delete) || (keysym == XK_KP_Delete)) {
    // I forget whether it should be Super-Shift-Delete or Super-
    // Ctrl-Delete according to the system, so let's allow both.
    action_delete_workspace_if_empty (popup, shifted | ctrled, time);
  }
  else if (keysym == XK_Tab) {
    action_change_active_window_by_stacking_order (popup, shifted, time);
  }
  else if (keysym == XK_Escape) {
    action_close_active_window (popup, ctrled, time);
  }
  else if (keysym == XK_F1) {
    action_change_active_workspace (popup,  0, shifted, ctrled, time);
  }
  else if (keysym == XK_F2) {
    action_change_active_workspace (popup,  1, shifted, ctrled, time);
  }
  else if (keysym == XK_F3) {
    action_change_active_workspace (popup,  2, shifted, ctrled, time);
  }
  else if (keysym == XK_F4) {
    action_change_active_workspace (popup,  3, shifted, ctrled, time);
  }
  else if (keysym == XK_F5) {
    action_change_active_workspace (popup,  4, shifted, ctrled, time);
  }
  else if (keysym == XK_F6) {
    action_change_active_workspace (popup,  5, shifted, ctrled, time);
  }
  else if (keysym == XK_F7) {
    action_change_active_workspace (popup,  6, shifted, ctrled, time);
  }
  else if (keysym == XK_F8) {
    action_change_active_workspace (popup,  7, shifted, ctrled, time);
  }
  else if (keysym == XK_F9) {
    action_change_active_workspace (popup,  8, shifted, ctrled, time);
  }
  else if (keysym == XK_F10) {
    action_change_active_workspace (popup,  9, shifted, ctrled, time);
  }
  else if (keysym == XK_F11) {
    action_change_active_workspace (popup, 10, shifted, ctrled, time);
  }
  else if (keysym == XK_F12) {
    action_change_active_workspace (popup, 11, shifted, ctrled, time);
  }
  else if ((keysym == XK_Super_L) || (keysym == XK_Super_R)) {
    action_change_xinerama (popup, time);
  }
  else if ((keysym == XK_Return) || (keysym == XK_ISO_Enter) || (keysym == XK_KP_Enter)) {
    action_activate_next_window (popup, shifted, time);
  }
  else if (keysym == XK_BackSpace) {
    if (popup->search_text_label != NULL) {
      old_search_text = gtk_label_get_text (GTK_LABEL (popup->search_text_label));
      n = strlen (old_search_text);
      if (n > 0) {
        new_search_text = g_strndup (old_search_text, n - 1);
        gtk_label_set_text (GTK_LABEL (popup->search_text_label), new_search_text);
        g_free (new_search_text);
      }

      update_search (popup);
    }
  }
  else {
    if (key_string[0] != '\0') {
      if (popup->search_text_label == NULL) {
        search_widget_create (popup);
        gtk_widget_queue_draw (popup->window);
      }
      old_search_text = gtk_label_get_text (GTK_LABEL (popup->search_text_label));
      new_search_text = g_strdup_printf ("%s%s", old_search_text, key_string);
      gtk_label_set_text (GTK_LABEL (popup->search_text_label), new_search_text);
      g_free (new_search_text);

      update_search (popup);
    }
  }
}
