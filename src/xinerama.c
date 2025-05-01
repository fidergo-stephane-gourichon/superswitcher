// Copyright (c) 2006 Nigel Tao.
// Licenced under the GNU General Public Licence (GPL) version 2.

#include "xinerama.h"

#include <gdk/gdk.h>
#include <glib.h>
#include <libwnck/libwnck.h>

#ifdef HAVE_XINERAMA
#include <X11/extensions/Xinerama.h>
#endif

#include "window.h"

//------------------------------------------------------------------------------

SSXinerama *
ss_xinerama_new (Display *x_display, Window x_root_window)
{
  int num_screens;
  int minimum_width;
  SSXineramaScreen *screens;
  SSXinerama *xinerama;

  minimum_width = 0;

#ifdef HAVE_XINERAMA
  gboolean xinerama_is_active = XineramaIsActive (x_display);
#else
  gboolean xinerama_is_active = FALSE;
#endif

  if (xinerama_is_active) {
#ifdef HAVE_XINERAMA
    XineramaScreenInfo *xsi_array =
      XineramaQueryScreens(x_display, &num_screens);
    screens = g_new (SSXineramaScreen, num_screens);

    int i;
    for (i = 0; i < num_screens; i++) {
      XineramaScreenInfo *xsi = &xsi_array[i];
      screens[i].x = xsi->x_org;
      screens[i].y = xsi->y_org;
      screens[i].width  = xsi->width;
      screens[i].height = xsi->height;
      if (i == 0) {
        minimum_width = screens[0].width;
      } else {
        minimum_width = MIN (minimum_width, screens[i].width);
      }
    }
    XFree (xsi_array);
#endif
  } else {
    num_screens = 1;
    int x_screen = DefaultScreen (x_display);
    screens = g_new (SSXineramaScreen, num_screens);
    screens[0].x = 0;
    screens[0].y = 0;
    screens[0].width  = DisplayWidth (x_display, x_screen);
    screens[0].height = DisplayHeight (x_display, x_screen);
    minimum_width = screens[0].width;
  }

  xinerama = g_new (SSXinerama, 1);
  xinerama->x_display = x_display;
  xinerama->x_root_window = x_root_window;
  xinerama->num_screens = num_screens;
  xinerama->screens = screens;
  xinerama->minimum_width = minimum_width;
  xinerama->net_frame_extents_atom = XInternAtom (x_display, "_NET_FRAME_EXTENTS", True);
  return xinerama;
}

//------------------------------------------------------------------------------

void
ss_xinerama_free(SSXinerama *xinerama)
{
    if (xinerama == NULL) {
        return;
    }
    if (xinerama->screens != NULL) {
        g_free(xinerama->screens);
    }
    g_free(xinerama);
}

//------------------------------------------------------------------------------

static int
get_best_screen (SSXinerama *xinerama, SSWindow *window)
{
  GdkRectangle r;
  SSXineramaScreen *xs;
  int i;
  int rightmost_left;
  int leftmost_right;
  int bottommost_top;
  int topmost_bottom;
  int dx, dy;
  int overlapping_area;
  int best_overlapping_area;
  int best_screen;

  best_overlapping_area = 0;
  best_screen = 0;

  // first, find out which Xinerama screen the window is most on
  wnck_window_get_geometry (window->wnck_window, &r.x, &r.y, &r.width, &r.height);
  for (i = 0; i < xinerama->num_screens; i++) {
    xs = &xinerama->screens[i];
    rightmost_left = MAX (xs->x, r.x);
    leftmost_right = MIN (xs->x + xs->width, r.x + r.width);
    bottommost_top = MAX (xs->y, r.y);
    topmost_bottom = MIN (xs->y + xs->height, r.y + r.height);

    dx = leftmost_right - rightmost_left;
    dy = topmost_bottom - bottommost_top;
    if (dx < 0 || dy < 0) {
      overlapping_area = 0;
    } else {
      overlapping_area = dx * dy;
    }

    if (i == 0 || overlapping_area > best_overlapping_area) {
      best_overlapping_area = overlapping_area;
      best_screen = i;
    }
  }

  return best_screen;
}

//------------------------------------------------------------------------------

static void
ss_xinerama_get_frame_coordinates (SSXinerama *xinerama, SSWindow *window,
                                   int *out_x, int *out_y, int *out_width, int *out_height)
{
  Window x_window, x_root_window, an_ignored_window;
  int x, y, x_wrt_root, y_wrt_root;
  unsigned int width, height, border_width, depth;
  int frame_left, frame_right, frame_top, frame_bottom;

  if (xinerama == NULL || window == NULL) {
    *out_x = *out_y = *out_width = *out_height = -1;
    return;
  }

  // Get the window's co-ordinates...
  x_window = wnck_window_get_xid (window->wnck_window);
  XGetGeometry (xinerama->x_display, x_window, &x_root_window, &x, &y,
                &width, &height, &border_width, &depth);

  // ...with respect to (wrt) the root X Window...
  XTranslateCoordinates (xinerama->x_display, x_window, x_root_window, 0, 0,
                         &x_wrt_root, &y_wrt_root, &an_ignored_window);

  // ...and adjust for the _NET_FRAME_EXTENTS.
  ss_xinerama_get_frame_extents (
      xinerama, window, &frame_left, &frame_right, &frame_top, &frame_bottom);

  *out_x = x_wrt_root - frame_left;
  *out_y = y_wrt_root - frame_top;
  *out_width  = (unsigned int) width  + frame_left + frame_right;
  *out_height = (unsigned int) height + frame_top  + frame_bottom;
}

//------------------------------------------------------------------------------

void
ss_xinerama_get_frame_extents (SSXinerama *xinerama, SSWindow *window,
                               int *out_left, int *out_right,
                               int *out_top, int *out_bottom)
{
  // XGetWindowProperty stuff
  Atom actual_type;
  int actual_format;
  unsigned long nitems;
  unsigned long bytes_remaining;
  unsigned char *data;
  long *data_as_long;
  int status;

  if (xinerama == NULL || window == NULL) {
    *out_left   = 0;
    *out_right  = 0;
    *out_top    = 0;
    *out_bottom = 0;
    return;
  }

  // Find the _NET_FRAME_EXTENTS, also known as the window border, including
  // the titlebar and resize grippies.
  status = XGetWindowProperty(
    xinerama->x_display,
    wnck_window_get_xid (window->wnck_window),
    xinerama->net_frame_extents_atom,
    0,      // long_offset
    4,      // long_length - we expect 4 32-bit values for _NET_FRAME_EXTENTS
    False,  // delete
    AnyPropertyType,
    &actual_type,
    &actual_format,
    &nitems,
    &bytes_remaining,
    &data);

  *out_left = *out_right = *out_top = *out_bottom = 0;
  if (status == Success) {
    if ((nitems == 4) && (bytes_remaining == 0)) {
      // Hoop-jumping to avoid gcc's "dereferencing type-punned pointer" warning
      data_as_long = (long *) ((void *) data);
      *out_left   = (int) *(data_as_long++);
      *out_right  = (int) *(data_as_long++);
      *out_top    = (int) *(data_as_long++);
      *out_bottom = (int) *(data_as_long++);
    }
    XFree (data);
  }
}

//------------------------------------------------------------------------------

void
ss_xinerama_move_to_next_screen (SSXinerama *xinerama, SSWindow *window)
{
  int best_screen;
  SSXineramaScreen *xs;
  int dx, dy;
  gboolean is_maximized;

  // Co-ordinates of a window's frame (as set by the window manager, such as
  // metacity).
  int fx, fy, fw, fh;

  if (xinerama->num_screens <= 1) {
    return;
  }
  if (window == NULL) {
    return;
  }

  // In metacity, you can't move a maximized window with XMoveWindow.  Our
  // workaround is to temporarily unmaximize, move, and re-maximize.
  is_maximized = wnck_window_is_maximized (window->wnck_window);
  if (is_maximized) {
    wnck_window_unmaximize (window->wnck_window);
  }

  // Get the window co-ordinates wrt the screen.
  best_screen = get_best_screen (xinerama, window);
  xs = &xinerama->screens[best_screen];
  ss_xinerama_get_frame_coordinates (xinerama, window, &fx, &fy, &fw, &fh);
  dx = fx - xs->x;
  dy = fy - xs->y;

  // Now move to the next screen.
  best_screen = (best_screen + 1) % xinerama->num_screens;
  xs = &xinerama->screens[best_screen];
  // I move the window to the desired x,y of its *frame* window on the other
  // (Xinerama) screen.  This is a little tricksy-ish, but it's what works
  // with metacity.  This may be totally broken on other window managers.
  XMoveWindow (xinerama->x_display, wnck_window_get_xid (window->wnck_window),
               xs->x + dx, xs->y + dy);

  if (is_maximized) {
    wnck_window_maximize (window->wnck_window);
  }
}
