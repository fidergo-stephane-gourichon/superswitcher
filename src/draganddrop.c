// Copyright (c) 2006 Nigel Tao.
// Licenced under the GNU General Public Licence (GPL) version 2.

#include "draganddrop.h"

#include <gtk/gtk.h>

#include "screen.h"
#include "window.h"
#include "workspace.h"

//------------------------------------------------------------------------------

static void
ss_draganddrop_reset (SSDragAndDrop *dnd)
{
  dnd->is_dragging = FALSE;
  dnd->drag_start_x = -1;
  dnd->drag_start_y = -1;
  dnd->drag_start_window = NULL;
  dnd->drag_start_workspace = NULL;
  dnd->drag_start_widget = NULL;
  dnd->drag_x = -1;
  dnd->drag_y = -1;
  dnd->drag_workspace = NULL;
  dnd->new_window_index = -1;
}

//------------------------------------------------------------------------------

SSDragAndDrop *
ss_draganddrop_new (SSScreen *screen)
{
  SSDragAndDrop *dnd;
  dnd = g_new (SSDragAndDrop, 1);
  dnd->screen = screen;
  ss_draganddrop_reset (dnd);
  return dnd;
}

//------------------------------------------------------------------------------

void
ss_draganddrop_free (SSDragAndDrop *dnd)
{
  if (dnd == NULL) {
    return;
  }
  g_free (dnd);
}

//------------------------------------------------------------------------------

static void
get_pointer_position (GtkWidget *widget, int *x, int *y)
{
  GdkDevice *pointer;
  pointer = gdk_seat_get_pointer (gdk_display_get_default_seat (gtk_widget_get_display (widget)));
  gdk_window_get_device_position (gtk_widget_get_window (widget), pointer, x, y, NULL);
}

//------------------------------------------------------------------------------

void
ss_draganddrop_on_motion (SSDragAndDrop *dnd)
{
  int x, y;
  if (dnd->drag_start_widget == NULL) {
    return;
  }
  get_pointer_position (dnd->drag_start_widget, &x, &y);

  if ((!dnd->is_dragging) &&
    (gtk_drag_check_threshold (dnd->drag_start_widget,
    dnd->drag_start_x, dnd->drag_start_y, x, y))) {

    dnd->is_dragging = TRUE;
  }
  if (dnd->is_dragging) {
    dnd->drag_x = x;
    dnd->drag_y = y;
    dnd->drag_workspace = ss_screen_find_workspace_near_point (dnd->screen, x, y);
    if (dnd->drag_workspace != NULL) {
      dnd->new_window_index =
        ss_workspace_find_index_near_point (
        dnd->drag_workspace, x, y);

      gtk_widget_queue_draw (gtk_widget_get_toplevel (dnd->drag_start_widget));
    } else {
      dnd->new_window_index = -1;
    }
  }
}

//------------------------------------------------------------------------------

void
ss_draganddrop_on_release (SSDragAndDrop *dnd)
{
  if (dnd->is_dragging) {
    if (dnd->drag_workspace != NULL && dnd->drag_start_widget != NULL) {
      gtk_widget_queue_draw (gtk_widget_get_toplevel (dnd->drag_start_widget));
    }
  }
  ss_draganddrop_reset (dnd);
}

//------------------------------------------------------------------------------

void
ss_draganddrop_start (SSDragAndDrop *dnd, SSWindow *window, SSWorkspace *workspace)
{
  GtkWidget *w;
  if (window != NULL) {
    w = window->widget;
  } else if (workspace != NULL) {
    w = workspace->widget;
  } else {
    w = NULL;
  }
  dnd->drag_start_window = window;
  dnd->drag_start_workspace = workspace;
  dnd->drag_start_widget = w;
  if (w != NULL) {
    get_pointer_position (w, &(dnd->drag_start_x), &(dnd->drag_start_y));
  } else {
    dnd->drag_start_x = -1;
    dnd->drag_start_y = -1;
  }
}
