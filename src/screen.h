// Copyright (c) 2006 Nigel Tao.
// Licenced under the GNU General Public Licence (GPL) version 2.

#ifndef SUPERSWITCHER_SCREEN_H
#define SUPERSWITCHER_SCREEN_H

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>
#include <libwnck/libwnck.h>
#include <X11/Xlib.h>

#include "forward_declarations.h"

#define SS_TYPE_SCREEN            (ss_screen_get_type ())
#define SS_SCREEN(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SS_TYPE_SCREEN, SSScreen))
#define SS_SCREEN_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  SS_TYPE_SCREEN, SSScreenClass))
#define SS_IS_SCREEN(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SS_TYPE_SCREEN))
#define SS_IS_SCREEN_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  SS_TYPE_SCREEN))
#define SS_SCREEN_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  SS_TYPE_SCREEN, SSScreenClass))

struct _SSScreen {
  GObject   parent_instance; // Unused.

  WnckScreen *   wnck_screen;
  SSXinerama *   xinerama;
  int            screen_width;
  int            screen_height;
  double         screen_aspect;

  // The widget is also the workspace_container
  GtkWidget *   widget;

  GList *   workspaces;
  int       num_workspaces;

  SSWindow *      active_window;
  SSWorkspace *   active_workspace;
  int             active_workspace_id;

  GList *    wnck_windows_in_stacking_order;
  gboolean   should_ignore_next_window_stacking_change;

  int   num_search_matches;

  SSDragAndDrop *   drag_and_drop;

  int   label_max_width_chars;

#ifndef HAVE_GTK_2_11
  GtkTooltips *   tooltips;
#endif

  gboolean   pointer_needs_recentering_on_focus_change;
};

typedef struct _SSScreenClass SSScreenClass;
struct _SSScreenClass {
  GObjectClass   parent_class; // Unused.
};

GType        ss_screen_get_type   (void);
SSScreen *   ss_screen_new        (WnckScreen *wnck_screen, Display *x_display, Window x_root_window);

SSWorkspace *   ss_screen_get_nth_workspace   (SSScreen *screen, int n);

void   ss_screen_activate_next_window                     (SSScreen *screen, gboolean backwards, guint32 time);
void   ss_screen_activate_next_window_in_stacking_order   (SSScreen *screen, gboolean backwards, guint32 time);
void   ss_screen_change_active_workspace                  (SSScreen *screen, int n, gboolean also_bring_active_window, gboolean all_not_just_current_window, guint32 time);
void   ss_screen_change_active_workspace_by_delta         (SSScreen *screen, int delta, gboolean also_bring_active_window, gboolean all_not_just_current_window, guint32 time);
void   ss_screen_change_active_workspace_to               (SSScreen *screen, WnckWorkspace *wnck_workspace, int viewport, gboolean also_bring_active_window, gboolean all_not_just_current_window, guint32 time);
void   ss_screen_update_search                            (SSScreen *screen, const char *query);
void   ss_screen_update_wnck_windows_in_stacking_order    (SSScreen *screen);

SSWorkspace *   ss_screen_get_workspace_for_wnck_window   (SSScreen *screen, WnckWindow *wnck_window);

SSWorkspace *   ss_screen_find_workspace_near_point   (SSScreen *screen, int x, int y);

void update_window_label_width (SSScreen *screen);
#endif
