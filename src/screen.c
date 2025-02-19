// Copyright (c) 2006 Nigel Tao.
// Licenced under the GNU General Public Licence (GPL) version 2.

#include "screen.h"

#include <libwnck/libwnck.h>
#include "string.h"

#ifdef HAVE_GCONF
#include <gconf/gconf-client.h>
#endif

#include "draganddrop.h"
#include "window.h"
#include "workspace.h"
#include "xinerama.h"

//------------------------------------------------------------------------------

#define NUMBER_OF_F_KEYS 12

static char *f_keys[] = {
  "F1", "F2", "F3", "F4", "F5", "F6",
  "F7", "F8", "F9", "F10", "F11", "F12"
};

//------------------------------------------------------------------------------

static guint active_window_changed_signal;
static guint active_workspace_changed_signal;
static guint window_closed_signal;
static guint window_opened_signal;
static guint workspace_created_signal;
static guint workspace_destroyed_signal;

#ifdef HAVE_GCONF
static GConfClient *gconf_client = NULL;
static guint gconf_focus_mode_notify_id = 0;
#endif

//------------------------------------------------------------------------------

static SSWindow *
get_ss_window_from_wnck_window (SSScreen *screen, WnckWindow *wnck_window)
{
  gulong xid;
  SSWorkspace *workspace;
  SSWindow *window;
  GList *i;
  GList *j;

  if (wnck_window == NULL) {
    return NULL;
  }

  xid = wnck_window_get_xid (wnck_window);

  // Walk through the list of list of windows, matching by X ID.
  for (i = screen->workspaces; i; i = i->next) {
    workspace = (SSWorkspace *) i->data;
    for (j = workspace->windows; j; j = j->next) {
      window = (SSWindow *) j->data;
      if (xid == wnck_window_get_xid (window->wnck_window)) {
        return window;
      }
    }
  }

  return NULL;
}

//------------------------------------------------------------------------------

static SSWorkspace *
get_ss_workspace_from_wnck_workspace (SSScreen *screen, WnckWorkspace *wnck_workspace, int viewport)
{
  SSWorkspace *workspace;
  GList *i;

  if (wnck_workspace == NULL) {
    return NULL;
  }

  if (window_manager_uses_viewports) {
    return (SSWorkspace *) g_list_nth_data (screen->workspaces, viewport);
  }

  for (i = screen->workspaces; i; i = i->next) {
    workspace = (SSWorkspace *) i->data;
    if (wnck_workspace == workspace->wnck_workspace) {
      return workspace;
    }
  }

  return NULL;
}

//------------------------------------------------------------------------------

SSWorkspace *
ss_screen_get_workspace_for_wnck_window (SSScreen *screen, WnckWindow *wnck_window)
{
  int x, y, w, h;
  int vx;
  int n;
  if (window_manager_uses_viewports) {
    wnck_window_get_geometry (wnck_window, &x, &y, &w, &h);
    vx = wnck_workspace_get_viewport_x (wnck_window_get_workspace (wnck_window));
    n = (vx + x + (w / 2)) / screen->screen_width;
    if (n < 0) {
      n = 0;
    } else if (n >= screen->num_workspaces) {
      n = screen->num_workspaces - 1;
    }
  } else {
    n = wnck_workspace_get_number (wnck_window_get_workspace (wnck_window));
  }
  return ss_screen_get_nth_workspace (screen, n);
}

//------------------------------------------------------------------------------

SSWorkspace *
ss_screen_get_nth_workspace (SSScreen *screen, int n)
{
  return (SSWorkspace *) g_list_nth_data (screen->workspaces, n);
}

//------------------------------------------------------------------------------

void
ss_screen_change_active_workspace_to (SSScreen *screen, WnckWorkspace *wnck_workspace, int viewport,
  gboolean also_bring_active_window, gboolean all_not_just_current_window, guint32 time)
{
  GList *i;
  SSWindow *window;
  SSWorkspace *workspace;

  if (also_bring_active_window) {
    workspace = get_ss_workspace_from_wnck_workspace (screen, wnck_workspace, viewport);
    if (all_not_just_current_window) {
      if (screen->active_workspace != NULL) {
        for (i = screen->active_workspace->windows; i; i = i->next) {
          window = (SSWindow *) i->data;
          ss_window_move_to_workspace (window, workspace);
        }
      }
    } else {
      if (screen->active_window != NULL) {
        ss_window_move_to_workspace (screen->active_window, workspace);
      }
    }
  }

  if (window_manager_uses_viewports) {
    wnck_screen_move_viewport (screen->wnck_screen, screen->screen_width * viewport, 0);
  } else {
    wnck_workspace_activate (wnck_workspace, time);
  }
}

//------------------------------------------------------------------------------

void
ss_screen_change_active_workspace_by_delta (SSScreen *screen, int delta,
  gboolean also_bring_active_window, gboolean all_not_just_current_window, guint32 time)
{
  WnckWorkspace *wnck_workspace;
  int w, n;

  w = screen->active_workspace_id == -1 ? 0 : screen->active_workspace_id;
  w += delta;
  n = screen->num_workspaces;

  while (w < 0) {
    w += n;
  }
  while (w >= n) {
    w -= n;
  }

  wnck_workspace = wnck_screen_get_workspace (screen->wnck_screen, window_manager_uses_viewports ? 0 : w);
  ss_screen_change_active_workspace_to (screen, wnck_workspace, window_manager_uses_viewports ? w : 0,
    also_bring_active_window, all_not_just_current_window, time);
}

//------------------------------------------------------------------------------

void
ss_screen_change_active_workspace (SSScreen *screen, int n, gboolean also_bring_active_window,
  gboolean all_not_just_current_window, guint32 time)
{
  WnckWorkspace *wnck_workspace;

  if ((n >= 0) && (n < screen->num_workspaces)) {
    wnck_workspace = wnck_screen_get_workspace (screen->wnck_screen, window_manager_uses_viewports ? 0 : n);
    ss_screen_change_active_workspace_to (screen, wnck_workspace, window_manager_uses_viewports ? n : 0,
      also_bring_active_window, all_not_just_current_window, time);
  }
}

//------------------------------------------------------------------------------

static void
update_search (SSScreen *screen, SSWindow *window, gchar** terms)
{
  gboolean matched;
  char* title;
  gchar* term;
  int t;

  title = (char *) wnck_window_get_name (window->wnck_window);
  if (title != NULL) {
    matched = TRUE;
    title = g_ascii_strdown (title, strlen (title));
    t = 0;
    while (terms[t] != NULL) {
      term = terms[t];
      t++;
      if (strlen(term) == 0) {
        continue;
      }
      if (g_strrstr (title, term) == NULL) {
        matched = FALSE;
        break;
      }
    }
    g_free (title);
  } else {
    matched = FALSE;
  }

  ss_window_set_sensitive (window, matched);
  if (matched) {
    screen->num_search_matches++;
  }
}

//------------------------------------------------------------------------------

void
ss_screen_update_search (SSScreen *screen, const char *query)
{
  char *normalized_query;
  gchar** terms;
  SSWorkspace *workspace;
  SSWindow *window;
  GList *i;
  GList *j;

  normalized_query = g_ascii_strdown (query, strlen (query));
  terms = g_strsplit (normalized_query, " ", 0);
  screen->num_search_matches = 0;

  for (i = screen->workspaces; i; i = i->next) {
    workspace = (SSWorkspace *) i->data;
    for (j = workspace->windows; j; j = j->next) {
      window = (SSWindow *) j->data;

      update_search (screen, window, terms);
    }
  }

  g_strfreev (terms);
  g_free (normalized_query);
}

//------------------------------------------------------------------------------

void
ss_screen_activate_next_window (SSScreen *screen, gboolean backwards, guint32 time)
{
  SSWorkspace *workspace;
  SSWindow *window;
  GList *i;
  GList *j;

  SSWindow *first_sensitive_window;
  SSWindow *previous_sensitive_window;
  gboolean should_activate_last_sensitive_window;
  gboolean should_activate_next_sensitive_window;
  gboolean found_active_window;

  gboolean also_warp_pointer_if_necessary;

  first_sensitive_window    = NULL;
  previous_sensitive_window = NULL;
  should_activate_last_sensitive_window = FALSE;
  should_activate_next_sensitive_window = FALSE;
  found_active_window = FALSE;
  also_warp_pointer_if_necessary = TRUE;

  for (i = screen->workspaces; i; i = i->next) {
    workspace = (SSWorkspace *) i->data;
    if ((screen->active_window == NULL) && (screen->active_workspace == workspace)) {
      if (backwards) {
        if (previous_sensitive_window == NULL) {
          should_activate_last_sensitive_window = TRUE;
        } else {
          ss_window_activate_workspace_and_window (previous_sensitive_window, time,
            also_warp_pointer_if_necessary);
          return;
        }
      } else {
        should_activate_next_sensitive_window = TRUE;
      }
    }
    for (j = workspace->windows; j; j = j->next) {
      window = (SSWindow *) j->data;

      if (screen->active_window != NULL) {
        found_active_window = (window == screen->active_window);
      }

      if (backwards && found_active_window) {
        if (previous_sensitive_window == NULL) {
          should_activate_last_sensitive_window = TRUE;
        } else {
          ss_window_activate_workspace_and_window (previous_sensitive_window, time,
            also_warp_pointer_if_necessary);
          return;
        }
      }

      if (window->sensitive) {
        if (should_activate_next_sensitive_window) {
          ss_window_activate_workspace_and_window (window, time,
            also_warp_pointer_if_necessary);
          return;
        }

        previous_sensitive_window = window;

        if (first_sensitive_window == NULL) {
          first_sensitive_window = window;
        }
      }

      if (!backwards && found_active_window) {
        should_activate_next_sensitive_window = TRUE;
      }
    }
  }

  if (should_activate_next_sensitive_window) {
    ss_window_activate_workspace_and_window (first_sensitive_window, time,
      also_warp_pointer_if_necessary);
    return;
  }

  if (should_activate_last_sensitive_window) {
    ss_window_activate_workspace_and_window (previous_sensitive_window, time,
      also_warp_pointer_if_necessary);
    return;
  }
}

//------------------------------------------------------------------------------

void
ss_screen_activate_next_window_in_stacking_order (SSScreen *screen, gboolean backwards, guint32 time)
{
  WnckWindow *active_wnck_window;
  WnckWindow *wnck_window;
  gboolean ww_is_aww;
  WnckWorkspace *active_wnck_workspace;
  WnckWorkspace *wnck_workspace;
  int active_workspace_id;
  int workspace_id;
  GList *i;
  int num_windows;

  WnckWindow *first_eligible_wnck_window;
  WnckWindow *previous_eligible_wnck_window;
  gboolean should_activate_last_eligible_wnck_window;
  gboolean should_activate_next_eligible_wnck_window;

  gboolean also_warp_pointer_if_necessary;
  also_warp_pointer_if_necessary = TRUE;

  num_windows = g_list_length (screen->active_workspace->windows);
  if (num_windows == 0) {
    return;
  }

  // TODO - this is probably an ugly hack that's exposed to a potential
  // race condition.  A better way to go about this would be to suppress
  // the window_stacking_change signal, I suppose.
  // For example, if a window gets opened in between setting this flag
  // and in the event handler on_window_stacking_changed, then the
  // window_stacking_order list will be copied at the wrong point,
  // possibly.
  screen->should_ignore_next_window_stacking_change = TRUE;

  first_eligible_wnck_window    = NULL;
  previous_eligible_wnck_window = NULL;
  should_activate_last_eligible_wnck_window = FALSE;
  should_activate_next_eligible_wnck_window = FALSE;

  if (screen->active_window != NULL) {
    active_wnck_window = screen->active_window->wnck_window;
  } else {
    active_wnck_window = NULL;
  }
  active_wnck_workspace = wnck_screen_get_active_workspace (screen->wnck_screen);
  active_workspace_id = (active_wnck_workspace != NULL) ? wnck_workspace_get_number (active_wnck_workspace) : -1;

  // Note that wnck_windows_in_stacking_order is in bottom-to-top
  // order, so that if we're searching forwards (just like Alt-Tab)
  // then we want the previous_eligible_wnck_window.  This is the
  // opposite ordering than in ss_screen_activate_next_window.
  for (i = screen->wnck_windows_in_stacking_order; i; i = i->next) {
    wnck_window = (WnckWindow *) i->data;
    wnck_workspace = wnck_window_get_workspace (wnck_window);
    workspace_id = (wnck_workspace != NULL) ? wnck_workspace_get_number (wnck_workspace) : -1;

    if (workspace_id != active_workspace_id) {
      continue;
    }

    if (active_wnck_window != NULL) {
      ww_is_aww = (wnck_window_get_xid (wnck_window) ==
        wnck_window_get_xid (active_wnck_window));
    } else {
      ww_is_aww = FALSE;
    }

    // We've found the active window, and we're searching forwards.
    if (!backwards && ww_is_aww) {
      if (previous_eligible_wnck_window == NULL) {
        should_activate_last_eligible_wnck_window = TRUE;
      } else {
        ss_window_activate_window (get_ss_window_from_wnck_window (
          screen, previous_eligible_wnck_window), time,
          also_warp_pointer_if_necessary);
        return;
      }
    }

    if (should_activate_next_eligible_wnck_window) {
      ss_window_activate_window (get_ss_window_from_wnck_window (
        screen, wnck_window), time, also_warp_pointer_if_necessary);
      return;
    }

    previous_eligible_wnck_window = wnck_window;

    if (first_eligible_wnck_window == NULL) {
      first_eligible_wnck_window = wnck_window;
    }

    // We've found the active window, and we're searching backwards.
    if (backwards && ww_is_aww) {
      should_activate_next_eligible_wnck_window = TRUE;
    }
  }

  if (active_wnck_window == NULL) {
    if (backwards) {
      should_activate_next_eligible_wnck_window = TRUE;
    } else {
      should_activate_last_eligible_wnck_window = TRUE;
    }
  }

  if (should_activate_next_eligible_wnck_window) {
    ss_window_activate_window (get_ss_window_from_wnck_window (
      screen, first_eligible_wnck_window), time, also_warp_pointer_if_necessary);
    return;
  }

  if (should_activate_last_eligible_wnck_window) {
    ss_window_activate_window (get_ss_window_from_wnck_window (
      screen, previous_eligible_wnck_window), time, also_warp_pointer_if_necessary);
    return;
  }
}

//------------------------------------------------------------------------------

static void
update_workspace_titles (SSScreen *screen)
{
  SSWorkspace *workspace;
  GList *i;
  int j;

  for (j = 0, i = screen->workspaces; i; j++, i = i->next) {
    workspace = (SSWorkspace *) i->data;
    workspace->title = (j < NUMBER_OF_F_KEYS) ? f_keys[j] : "";
  }
}

//------------------------------------------------------------------------------

static void
update_for_active_window (SSScreen *screen)
{
  SSWindow *window;
  WnckWindow *wnck_window;
  wnck_window = wnck_screen_get_active_window (screen->wnck_screen);

  window = get_ss_window_from_wnck_window (screen, wnck_window);

  if (screen->active_window != window) {
    if (screen->active_window != NULL) {
      ss_window_set_selected (screen->active_window, FALSE);
    }
    screen->active_window = window;
    if (screen->active_window != NULL) {
      ss_window_set_selected (screen->active_window, TRUE);
    }
  }
}

//------------------------------------------------------------------------------

static void
update_for_active_workspace (SSScreen *screen)
{
  WnckWorkspace *wnck_workspace;

  if (window_manager_uses_viewports) {
    wnck_workspace = wnck_screen_get_workspace (screen->wnck_screen, 0);
    screen->active_workspace_id = wnck_workspace_get_viewport_x (wnck_workspace) / screen->screen_width;
    screen->active_workspace = ss_screen_get_nth_workspace
      (screen, screen->active_workspace_id);
  } else {
    wnck_workspace = wnck_screen_get_active_workspace (screen->wnck_screen);
    if (wnck_workspace) {
      screen->active_workspace_id = wnck_workspace_get_number (wnck_workspace);
      screen->active_workspace = ss_screen_get_nth_workspace
        (screen, screen->active_workspace_id);
    } else {
      screen->active_workspace_id = -1;
      screen->active_workspace = NULL;
    }
  }
}

//------------------------------------------------------------------------------

static SSWorkspace *
add_workspace_to_screen (SSScreen *screen, WnckWorkspace *wnck_workspace, int viewport)
{
  SSWorkspace *workspace;
  workspace = ss_workspace_new (screen, wnck_workspace, viewport);
  screen->workspaces = g_list_append (screen->workspaces, workspace);
  gtk_box_pack_start (GTK_BOX (screen->widget),
    workspace->widget, FALSE, FALSE, 0);
  return workspace;
}

//------------------------------------------------------------------------------

static SSWindow *
add_window_to_screen (SSScreen *screen, WnckWindow *wnck_window)
{
  SSWindow *window;
  SSWorkspace *workspace;
  WnckWorkspace *wnck_workspace;

  if (wnck_window_is_skip_pager (wnck_window)) {
    return NULL;
  }

  wnck_workspace = wnck_window_get_workspace (wnck_window);
  if (wnck_workspace == NULL) {
    // TODO - add it to the catch-all 'workspace' for e.g. those windows shown on all workspaces.
    return NULL;
  }
  workspace = ss_screen_get_workspace_for_wnck_window (screen, wnck_window);
  window = ss_window_new (workspace, wnck_window);
  if (wnck_window_is_active (wnck_window)) {
    if (screen->active_window != NULL) {
      ss_window_set_selected (screen->active_window, FALSE);
    }
    screen->active_window = window;
    if (screen->active_window != NULL) {
      ss_window_set_selected (screen->active_window, TRUE);
    }
  }
  ss_workspace_add_window (workspace, window);
  return window;
}

//------------------------------------------------------------------------------

#define g_logvar(VARNAME) g_debug( #VARNAME "=%d", VARNAME)

void
update_window_label_width (SSScreen *screen)
{
  PangoContext *context;
  PangoFontMetrics *metrics;
  int char_width;
  SSWorkspace *workspace;
  SSWindow *window;
  GList *i;
  GList *j;

  context = gtk_widget_get_pango_context (screen->widget);
  metrics = pango_context_get_metrics (context,
    screen->widget->style->font_desc, NULL);
  char_width = PANGO_PIXELS (pango_font_metrics_get_approximate_char_width (metrics));
  g_logvar(char_width);
  pango_font_metrics_unref (metrics);

  // On which screen is the mouse pointer? Is it the most
  // straightforward way to anticipate where gtk will put our popup?
  gint x,y;
  gdk_display_get_pointer(gdk_display_get_default(), NULL, &x, &y, NULL);
  g_debug("Mouse pointer at (%u,%u)\n", x, y);

  int screen_width = 800;
  {
    int matching_screen_count = 0;
    SSXinerama *xinerama = screen->xinerama;
    for (int i_s = 0; i_s < xinerama->num_screens; i_s++) {
      SSXineramaScreen *xs = &(xinerama->screens[i_s]);
      g_debug("Trying to match with screen at (%u,%u) dimensions (%u,%u).\n", xs->x, xs->y, xs->width, xs->height);
      if (x < xs->x) continue;
      if (x >= xs->x + xs->width) continue;
      if (y < xs->y) continue;
      if (y >= xs->y + xs->height) continue;
      matching_screen_count++;
      g_debug("Match count %u.\n", matching_screen_count);
      screen_width = xs->width;
    }

    if (matching_screen_count != 1) {
    g_warning("Not one screen contain coordinates (%u,%u) but %u.\n", x, y, matching_screen_count);
    }
  }

  // We'd like to cover all screen width...
  int window_width = screen_width;
  g_logvar(window_width);
  // But there are a few pixels of margin (should maybe compute them from gtk style, but is it even accessible here?)
  window_width -= 10;
  g_logvar(window_width);
  // So divide by the number of columns to get width per column.
  // Note: per C rules this rounds down, which is what we want.
  int column_width = window_width / screen->num_workspaces;
  g_logvar(column_width);
  // Substract 2 chars for the ellipsis.
  column_width -= 2*char_width;
  g_logvar(column_width);
  // Subtract off a bit for the icon and the margin.
  column_width -= 16;
  g_logvar(column_width);
  // The remainder is for the label.
  // Convert from pixels to chars.
  int width_in_chars = column_width * 5 / (7 * char_width);
  g_logvar(width_in_chars);
  // We compute a max label width for all desktops.

  // If a desktop (column) has a very long window title and the others
  // have short titles, we will truncate the long and waste space
  // because the short won't take it. It would make sense to compute
  // per-column limit.

  screen->label_max_width_chars = width_in_chars;

  for (i = screen->workspaces; i; i = i->next) {
    workspace = (SSWorkspace *) i->data;
    for (j = workspace->windows; j; j = j->next) {
      window = (SSWindow *) j->data;
      ss_window_update_label_max_width_chars (window);
    }
  }
}

//------------------------------------------------------------------------------

#ifdef HAVE_WNCK_2_19_3_1
static void
on_viewports_changed (WnckScreen *wnck_screen, gpointer data)
{
  SSScreen *screen;
  SSWindow *window;
  WnckWindow *wnck_window;
  GList *i;
  screen = (SSScreen *) data;
  if (!window_manager_uses_viewports) {
    return;
  }
  // TODO - don't segfault (via a bad widget) when the number of viewports change
  update_for_active_workspace (screen);
  for (i = wnck_screen_get_windows (screen->wnck_screen); i; i = i->next) {
    wnck_window = (WnckWindow *) i->data;
    window = get_ss_window_from_wnck_window (screen, wnck_window);
    if (window) {
      ss_window_update_for_new_workspace (
        window, ss_screen_get_workspace_for_wnck_window (screen, wnck_window));
    }
  }
  g_signal_emit (screen, active_workspace_changed_signal, 0, NULL);
}
#endif

//------------------------------------------------------------------------------

static void
#ifdef HAVE_WNCK_2_19_3_1
on_active_window_changed (WnckScreen *wnck_screen, WnckWindow *previous_window, gpointer data)
#else
on_active_window_changed (WnckScreen *wnck_screen, gpointer data)
#endif
{
  SSScreen *screen;
  screen = (SSScreen *) data;
  update_for_active_window (screen);
  g_signal_emit (screen, active_window_changed_signal, 0, NULL);
}

//------------------------------------------------------------------------------

static void
#ifdef HAVE_WNCK_2_19_3_1
on_active_workspace_changed (WnckScreen *wnck_screen, WnckWorkspace *previous_workspace, gpointer data)
#else
on_active_workspace_changed (WnckScreen *wnck_screen, gpointer data)
#endif
{
  SSScreen *screen;
  screen = (SSScreen *) data;
  update_for_active_workspace (screen);
  g_signal_emit (screen, active_workspace_changed_signal, 0, NULL);
}

//------------------------------------------------------------------------------

static void
on_window_closed (WnckScreen *wnck_screen, WnckWindow *wnck_window, gpointer data)
{
  SSScreen *screen;
  SSWindow *window;
  WnckWorkspace *wnck_workspace;
  int n;

  screen = (SSScreen *) data;
  window = get_ss_window_from_wnck_window (screen, wnck_window);
  if (window == NULL) {
    return;
  }
  wnck_workspace = wnck_window_get_workspace (wnck_window);
  if (wnck_workspace != NULL) {
    n = wnck_workspace_get_number (wnck_workspace);
    ss_workspace_remove_window (ss_screen_get_nth_workspace (screen, n), window);
  }

  if (screen->active_window == window) {
    screen->active_window = NULL;
  }

  g_signal_emit (screen, window_closed_signal, 0, window);
  ss_window_free (window);
}

//------------------------------------------------------------------------------

static void
on_window_opened (WnckScreen *wnck_screen, WnckWindow *wnck_window, gpointer data)
{
  SSScreen *screen;
  SSWindow *window;

  screen = (SSScreen *) data;
  window = add_window_to_screen (screen, wnck_window);
  if (window == NULL) {
    return;
  }

  g_signal_emit (screen, window_opened_signal, 0, window);
}

//------------------------------------------------------------------------------

void
ss_screen_update_wnck_windows_in_stacking_order (SSScreen *screen)
{
  WnckWindow *wnck_window;
  GList *i;

  if (screen->wnck_windows_in_stacking_order != NULL) {
    g_list_free (screen->wnck_windows_in_stacking_order);
    screen->wnck_windows_in_stacking_order = NULL;
  }

  // Make a copy of wnck_screen_get_windows_stacked (screen->wnck_screen),
  // filtering out those windows that we do not show.
  for (i = wnck_screen_get_windows_stacked (screen->wnck_screen); i; i = i->next) {
    wnck_window = (WnckWindow *) i->data;
    if (wnck_window_is_skip_pager (wnck_window)) {
      continue;
    }

    screen->wnck_windows_in_stacking_order = g_list_append
      (screen->wnck_windows_in_stacking_order, wnck_window);
  }

  for (i = screen->wnck_windows_in_stacking_order; i; i = i->next) {
    wnck_window = (WnckWindow *) i->data;
  }
}

//------------------------------------------------------------------------------

static void
on_window_stacking_changed (WnckScreen *wnck_screen, gpointer data)
{
  SSScreen *screen;
  screen = (SSScreen *) data;

  if (screen->should_ignore_next_window_stacking_change) {
    screen->should_ignore_next_window_stacking_change = FALSE;
    return;
  }

  ss_screen_update_wnck_windows_in_stacking_order (screen);
}

//------------------------------------------------------------------------------

SSWorkspace *
ss_screen_find_workspace_near_point (SSScreen *screen, int x, int y)
{
  GList *i;
  SSWorkspace *workspace;
  workspace = NULL;
  for (i = screen->workspaces; i; i = i->next) {
    workspace = (SSWorkspace *) i->data;
    if (x < (workspace->widget->allocation.x +
      workspace->widget->allocation.width +
      WORKSPACE_COLUMN_SPACING)) {
      return workspace;
    }
  }
  return workspace;
}

//------------------------------------------------------------------------------

static void
on_workspace_created (WnckScreen *wnck_screen, WnckWorkspace *wnck_workspace, gpointer data)
{
  SSScreen *screen;
  SSWorkspace *workspace;

  screen = (SSScreen *) data;
  screen->num_workspaces = wnck_screen_get_workspace_count (wnck_screen);
  workspace = add_workspace_to_screen (screen, wnck_workspace, 0);
  gtk_widget_show_all (workspace->widget);

  update_window_label_width (screen);
  update_workspace_titles (screen);
  g_signal_emit (screen, workspace_created_signal, 0, workspace);
  gtk_widget_queue_draw (gtk_widget_get_toplevel (screen->widget));
}

//------------------------------------------------------------------------------

static void
on_workspace_destroyed (WnckScreen *wnck_screen, WnckWorkspace *wnck_workspace, gpointer data)
{
  SSScreen *screen;
  SSWorkspace *workspace;

  screen = (SSScreen *) data;
  screen->num_workspaces -= 1;
  workspace = get_ss_workspace_from_wnck_workspace (screen, wnck_workspace, 0);
  screen->workspaces = g_list_remove (screen->workspaces, workspace);

  update_window_label_width (screen);
  update_workspace_titles (screen);
  g_signal_emit (screen, workspace_destroyed_signal, 0, workspace);
  gtk_container_remove (GTK_CONTAINER (screen->widget), workspace->widget);
  ss_workspace_free (workspace);
  gtk_widget_queue_draw (gtk_widget_get_toplevel (screen->widget));
}

//------------------------------------------------------------------------------

#ifdef HAVE_GCONF
static void
on_focus_mode_change (GConfClient *client, guint cn_id, GConfEntry *entry, gpointer data)
{
  SSScreen *screen;
  GConfValue *value;
  const char *s;

  screen = (SSScreen *) data;
  value = gconf_entry_get_value (entry);
  if (value && (value->type == GCONF_VALUE_STRING)) {
    s = gconf_value_get_string (value);
    screen->pointer_needs_recentering_on_focus_change =
      (g_ascii_strcasecmp (s, "sloppy") == 0)  ||
      (g_ascii_strcasecmp (s, "mouse") == 0);
  }
}
#endif

//------------------------------------------------------------------------------

#ifdef HAVE_GCONF
static void
load_from_gconf (SSScreen *screen)
{
  char *s;
  if (gconf_client != NULL) {
    return;
  }

  gconf_client = gconf_client_get_default ();
  gconf_client_add_dir (gconf_client, "/apps/metacity", GCONF_CLIENT_PRELOAD_NONE, NULL);
  gconf_focus_mode_notify_id = gconf_client_notify_add (gconf_client,
    "/apps/metacity/general/focus_mode", on_focus_mode_change, screen, NULL, NULL);
  s = gconf_client_get_string (gconf_client, "/apps/metacity/general/focus_mode", NULL);
  screen->pointer_needs_recentering_on_focus_change =
    (g_ascii_strcasecmp (s, "sloppy") == 0)  ||
    (g_ascii_strcasecmp (s, "mouse") == 0);
  g_free (s);
}
#endif

//------------------------------------------------------------------------------

static void
ss_screen_class_init (SSScreenClass *klass)
{
  GType wcs_types[1] = { G_TYPE_POINTER };

  active_window_changed_signal = g_signal_newv (
    "active-window-changed",
    G_TYPE_FROM_CLASS (klass),
    G_SIGNAL_RUN_LAST,
    NULL, NULL, NULL,
    g_cclosure_marshal_VOID__VOID,
    G_TYPE_NONE,
    0, NULL);

  active_workspace_changed_signal = g_signal_newv (
    "active-workspace-changed",
    G_TYPE_FROM_CLASS (klass),
    G_SIGNAL_RUN_LAST,
    NULL, NULL, NULL,
    g_cclosure_marshal_VOID__VOID,
    G_TYPE_NONE,
    0, NULL);

  window_closed_signal = g_signal_newv (
    "window_closed",
    G_TYPE_FROM_CLASS (klass),
    G_SIGNAL_RUN_LAST,
    NULL, NULL, NULL,
    g_cclosure_marshal_VOID__POINTER,
    G_TYPE_NONE,
    1, wcs_types);

  window_opened_signal = g_signal_newv (
    "window_opened",
    G_TYPE_FROM_CLASS (klass),
    G_SIGNAL_RUN_LAST,
    NULL, NULL, NULL,
    g_cclosure_marshal_VOID__POINTER,
    G_TYPE_NONE,
    1, wcs_types);

  workspace_created_signal = g_signal_newv (
    "workspace_created",
    G_TYPE_FROM_CLASS (klass),
    G_SIGNAL_RUN_LAST,
    NULL, NULL, NULL,
    g_cclosure_marshal_VOID__POINTER,
    G_TYPE_NONE,
    1, wcs_types);

  workspace_destroyed_signal = g_signal_newv (
    "workspace_destroyed",
    G_TYPE_FROM_CLASS (klass),
    G_SIGNAL_RUN_LAST,
    NULL, NULL, NULL,
    g_cclosure_marshal_VOID__POINTER,
    G_TYPE_NONE,
    1, wcs_types);
}

//------------------------------------------------------------------------------

GType
ss_screen_get_type (void)
{
  static GType type = 0;
  if (type == 0) {
    static const GTypeInfo info = {
      sizeof (SSScreenClass),
      NULL,   // base_init
      NULL,   // base_finalize
      (GClassInitFunc) ss_screen_class_init,
      NULL,   // class_finalize
      NULL,   // class_data
      sizeof (SSScreen),
      0,      // n_preallocs
      NULL    // instance_init
    };
    type = g_type_register_static (G_TYPE_OBJECT, "SSScreenType", &info, 0);
  }
  return type;
}

//------------------------------------------------------------------------------

static gboolean
does_window_manager_use_viewports (WnckScreen *wnck_screen)
{
  return 0 == g_ascii_strncasecmp (
    wnck_screen_get_window_manager_name (wnck_screen), "compiz", 6);
}

//------------------------------------------------------------------------------

static int
get_viewport_count (WnckScreen *wnck_screen)
{
  int screen_width;
  int workspace_width;
  screen_width = wnck_screen_get_width (wnck_screen);
  workspace_width = wnck_workspace_get_width (wnck_screen_get_workspace (wnck_screen, 0));
  return workspace_width / screen_width;
}

//------------------------------------------------------------------------------

SSScreen *
ss_screen_new (WnckScreen *wnck_screen, Display *x_display, Window x_root_window)
{
  SSScreen *screen;

  GList *wnck_windows;
  int i;

  wnck_screen_force_update (wnck_screen);
  window_manager_uses_viewports = does_window_manager_use_viewports (wnck_screen);

  screen = (SSScreen *) g_object_new (SS_TYPE_SCREEN, NULL);
  screen->wnck_screen = wnck_screen;
  screen->xinerama = ss_xinerama_new (x_display, x_root_window);
  screen->screen_width  = wnck_screen_get_width (wnck_screen);
  screen->screen_height = wnck_screen_get_height (wnck_screen);
  screen->screen_aspect = (double) screen->screen_height / (double) screen->screen_width;

  screen->widget = gtk_hbox_new (FALSE, 12);
  g_object_ref (screen->widget);
  gtk_container_set_border_width (GTK_CONTAINER (screen->widget), 6);

  screen->num_workspaces = window_manager_uses_viewports
    ? get_viewport_count (wnck_screen)
    : wnck_screen_get_workspace_count (wnck_screen);

  screen->active_window = NULL;
  screen->active_workspace = NULL;
  screen->active_workspace_id = -1;

  screen->wnck_windows_in_stacking_order = NULL;
  screen->should_ignore_next_window_stacking_change = FALSE;
  ss_screen_update_wnck_windows_in_stacking_order (screen);

  screen->num_search_matches = 0;

  screen->drag_and_drop = ss_draganddrop_new (screen);

  screen->label_max_width_chars = 256;
  update_window_label_width (screen);

#ifndef HAVE_GTK_2_11
  screen->tooltips = gtk_tooltips_new ();
#endif

  screen->pointer_needs_recentering_on_focus_change = FALSE;
#ifdef HAVE_GCONF
  load_from_gconf (screen);
#endif

  // Add existing workspaces, and then existing windows
  screen->workspaces = NULL;
  for (i = 0; i < screen->num_workspaces; i++) {
    if (window_manager_uses_viewports) {
      add_workspace_to_screen (screen, wnck_screen_get_workspace (wnck_screen, 0), i);
    } else {
      add_workspace_to_screen (screen, wnck_screen_get_workspace (wnck_screen, i), 0);
    }
  }
  update_workspace_titles (screen);

  wnck_windows = wnck_screen_get_windows (wnck_screen);
  for (; wnck_windows; wnck_windows = wnck_windows->next) {
    add_window_to_screen (screen, WNCK_WINDOW (wnck_windows->data));
  }

  // Listen for new workspaces, and new windows
  g_signal_connect (G_OBJECT (wnck_screen), "active_window_changed",
    G_CALLBACK (on_active_window_changed),
    screen);
  g_signal_connect (G_OBJECT (wnck_screen), "active_workspace_changed",
    G_CALLBACK (on_active_workspace_changed),
    screen);
#ifdef HAVE_WNCK_2_19_3_1
  g_signal_connect (G_OBJECT (wnck_screen), "viewports_changed",
    G_CALLBACK (on_viewports_changed),
    screen);
#endif
  g_signal_connect (G_OBJECT (wnck_screen), "window_closed",
    G_CALLBACK (on_window_closed),
    screen);
  g_signal_connect (G_OBJECT (wnck_screen), "window_opened",
    G_CALLBACK (on_window_opened),
    screen);
  g_signal_connect (G_OBJECT (wnck_screen), "window_stacking_changed",
    G_CALLBACK (on_window_stacking_changed),
    screen);
  g_signal_connect (G_OBJECT (wnck_screen), "workspace_created",
    G_CALLBACK (on_workspace_created),
    screen);
  g_signal_connect (G_OBJECT (wnck_screen), "workspace_destroyed",
    G_CALLBACK (on_workspace_destroyed),
    screen);

  update_for_active_workspace (screen);

  return screen;
}
