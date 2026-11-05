// Copyright (c) 2006 Nigel Tao.
// Licenced under the GNU General Public Licence (GPL) version 2.

#ifndef SUPERSWITCHER_WINDOW_H
#define SUPERSWITCHER_WINDOW_H

#include <gtk/gtk.h>
#include <libwnck/libwnck.h>

#include "forward_declarations.h"

struct _SSWindow {
  SSScreen *      screen;
  SSWorkspace *   workspace;
  WnckWindow *    wnck_window;

  GtkWidget *   widget;
  GtkWidget *   row;
  GtkWidget *   image;
  GtkWidget *   label;

#ifdef HAVE_XCOMPOSITE
  SSThumbnailer *   thumbnailer;
#endif

  gulong   signal_id_geometry_changed;
  gulong   signal_id_icon_changed;
  gulong   signal_id_name_changed;
  gulong   signal_id_state_changed;
  gulong   signal_id_workspace_changed;

  gboolean   sensitive;

  int   new_window_index;
};

SSWindow *   ss_window_new    (SSWorkspace *workspace, WnckWindow *wnck_window);
void         ss_window_free   (SSWindow *window);

void   ss_window_activate_window                 (SSWindow *window, guint32 time, gboolean also_warp_pointer_if_necessary);
void   ss_window_activate_workspace_and_window   (SSWindow *window, guint32 time, gboolean also_warp_pointer_if_necessary);
void   ss_window_move_to_workspace               (SSWindow *window, SSWorkspace *workspace);
void   ss_window_set_selected                    (SSWindow *window, gboolean selected);
void   ss_window_set_sensitive                   (SSWindow *window, gboolean sensitive);
void   ss_window_update_for_new_workspace        (SSWindow *window, SSWorkspace *new_workspace);
void   ss_window_update_label_max_width_chars    (SSWindow *window);

#endif
