// Copyright (c) 2006 Nigel Tao.
// Licenced under the GNU General Public Licence (GPL) version 2.

#ifdef HAVE_XCOMPOSITE

#include "thumbnailer.h"

#include "screen.h"
#include "window.h"
#include "workspace.h"
#include "xinerama.h"

#include <cairo-xlib.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/Xlib.h>

//------------------------------------------------------------------------------

#define THUMBNAIL_SIZE 48

//------------------------------------------------------------------------------

gboolean show_window_thumbnails = FALSE;

//------------------------------------------------------------------------------

gboolean
init_composite (void)
{
  Display *display;
  display = gdk_x11_get_default_xdisplay ();

  // First, check the Composite extension, then the Render extension.
  int error_base;
  int event_base;
  int version_major;
  int version_minor;

  if (!XCompositeQueryExtension (display, &event_base, &error_base)) {
    return FALSE;
  }

  // We need at least version 0.2, for XCompositeNameWindowPixmap.
  XCompositeQueryVersion (display, &version_major, &version_minor);
  if (version_major <= 0 && version_minor < 2) {
    return FALSE;
  }

  if (!XRenderQueryExtension (display, &event_base, &error_base)) {
    return FALSE;
  }

  // We need at least version 0.6, for XRenderSetPictureTransform.
  XRenderQueryVersion (display, &version_major, &version_minor);
  if (version_major <= 0 && version_minor < 6) {
    return FALSE;
  }

  XCompositeRedirectSubwindows (display,
      GDK_WINDOW_XID (gdk_get_default_root_window ()),
      CompositeRedirectAutomatic);
  return TRUE;
}

//------------------------------------------------------------------------------

gboolean
uninit_composite (void)
{
  XCompositeUnredirectSubwindows (gdk_x11_get_default_xdisplay (),
      GDK_WINDOW_XID (gdk_get_default_root_window ()),
      CompositeRedirectAutomatic);
  return TRUE;
}

//------------------------------------------------------------------------------

// This code can only run after thumbnailer's drawing_area has been realized,
// and after the main loop has run so that wnck_window is initialized.
static void
initialize_thumbnailer_pictures (SSThumbnailer *thumbnailer)
{
  Display *display;
  int x_screen;
  Visual *visual;
  XWindowAttributes window_attributes;
  XRenderPictureAttributes pa;

  display = gdk_x11_get_default_xdisplay ();
  x_screen = DefaultScreen (display);
  visual = DefaultVisual (display, x_screen);

  thumbnailer->thumbnail_pixmap = XCreatePixmap (display,
      RootWindow (display, x_screen), THUMBNAIL_SIZE, THUMBNAIL_SIZE,
      DefaultDepth (display, x_screen));
  thumbnailer->thumbnail_surface = cairo_xlib_surface_create (display,
      thumbnailer->thumbnail_pixmap, visual, THUMBNAIL_SIZE, THUMBNAIL_SIZE);

  thumbnailer->thumbnail_picture = XRenderCreatePicture (display,
      thumbnailer->thumbnail_pixmap, XRenderFindVisualFormat (display, visual), 0, NULL);

  // Problem: the format of a picture must match the depth of its drawable, and
  // windows with an alpha channel, which GTK 3 applications can have, do not
  // have the depth of the default visual.
  // Solution: use the visual of the window itself.
  XGetWindowAttributes (display, wnck_window_get_xid (thumbnailer->wnck_window),
      &window_attributes);
  pa.subwindow_mode = IncludeInferiors;
  thumbnailer->window_picture = XRenderCreatePicture (display,
      wnck_window_get_xid (thumbnailer->wnck_window),
      XRenderFindVisualFormat (display, window_attributes.visual),
      CPSubwindowMode, &pa);
  XRenderSetPictureFilter (display, thumbnailer->window_picture,
      "good", NULL, 0);
}

//------------------------------------------------------------------------------

static gboolean
on_draw (GtkWidget *widget, cairo_t *c, gpointer data)
{
  XTransform transform;
  double scale;
  int wx, wy, ww, wh;
  int frame_left, frame_right, frame_top, frame_bottom;
  int thumbnail_width, thumbnail_height;
  int offset_x, offset_y;
  SSWindow *window;
  SSThumbnailer *thumbnailer;
  Display *display;
  display = gdk_x11_get_default_xdisplay ();
  thumbnailer = (SSThumbnailer *) data;
  window = thumbnailer->window;

  if (thumbnailer->thumbnail_pixmap == None) {
    initialize_thumbnailer_pictures (thumbnailer);
  }

  ss_xinerama_get_frame_extents (
      // TODO - should we cut out the ->workspace in the line below?
      window->workspace->screen->xinerama, window,
      &frame_left, &frame_right, &frame_top, &frame_bottom);

  wnck_window_get_geometry (window->wnck_window, &wx, &wy, &ww, &wh);

  scale = ww > wh ? ww : wh;
  scale /= (double) THUMBNAIL_SIZE;
  thumbnail_width = thumbnail_height = THUMBNAIL_SIZE;
  if (ww > wh) {
    thumbnail_height = wh * THUMBNAIL_SIZE / ww;
  } else {
    thumbnail_width = ww * THUMBNAIL_SIZE / wh;
  }
  
  transform.matrix[0][0] = XDoubleToFixed (scale);
  transform.matrix[0][1] = XDoubleToFixed (0.0);
  transform.matrix[0][2] = XDoubleToFixed (-frame_left * (scale - 1.0));
  transform.matrix[1][0] = XDoubleToFixed (0.0);
  transform.matrix[1][1] = XDoubleToFixed (scale);
  transform.matrix[1][2] = XDoubleToFixed (-frame_top * (scale - 1.0));
  transform.matrix[2][0] = XDoubleToFixed (0.0);
  transform.matrix[2][1] = XDoubleToFixed (0.0);
  transform.matrix[2][2] = XDoubleToFixed (1.0);
  XRenderSetPictureTransform (display, thumbnailer->window_picture,
      &transform);

  XRenderComposite (display, PictOpSrc,
    thumbnailer->window_picture, None, thumbnailer->thumbnail_picture,
    0, 0, 0, 0, 0, 0, THUMBNAIL_SIZE, THUMBNAIL_SIZE);
  cairo_surface_mark_dirty (thumbnailer->thumbnail_surface);

  offset_x = (THUMBNAIL_SIZE - thumbnail_width) / 2;
  offset_y = (THUMBNAIL_SIZE - thumbnail_height) / 2;

  // The XRender result fills the pixmap from its top left corner.
  cairo_set_source_surface (c, thumbnailer->thumbnail_surface, offset_x, offset_y);
  cairo_rectangle (c, offset_x, offset_y, thumbnail_width, thumbnail_height);
  cairo_fill (c);

  cairo_set_source_rgb (c, 0, 0, 0);
  cairo_set_line_width (c, 1.0);
  cairo_rectangle (c, offset_x + 0.5, offset_y + 0.5, thumbnail_width - 1, thumbnail_height - 1);
  cairo_stroke (c);

  return FALSE;
}

//------------------------------------------------------------------------------

SSThumbnailer *
ss_thumbnailer_new (SSWindow *window, WnckWindow *wnck_window, GtkWidget *drawing_area)
{
  SSThumbnailer *t;

  gtk_widget_set_size_request (drawing_area, THUMBNAIL_SIZE, THUMBNAIL_SIZE);
  gtk_widget_set_app_paintable (drawing_area, TRUE);

  t = g_new (SSThumbnailer, 1);
  t->window = window;
  t->wnck_window = wnck_window;
  t->drawing_area = drawing_area;
  t->thumbnail_pixmap = None;
  t->thumbnail_surface = NULL;
  t->thumbnail_picture = None;
  t->window_picture = None;

  g_signal_connect (G_OBJECT (drawing_area), "draw",
                    G_CALLBACK (on_draw),
                    t);
  return t;
}

//------------------------------------------------------------------------------

void
ss_thumbnailer_free (SSThumbnailer *thumbnailer)
{
  Display *display;
  display = gdk_x11_get_default_xdisplay ();

  if (thumbnailer == NULL) {
    return;
  }

  if (thumbnailer->thumbnail_surface != NULL) {
    cairo_surface_destroy (thumbnailer->thumbnail_surface);
    thumbnailer->thumbnail_surface = NULL;
  }
  if (thumbnailer->thumbnail_picture != None) {
    XRenderFreePicture (display, thumbnailer->thumbnail_picture);
    thumbnailer->thumbnail_picture = None;
  }
  if (thumbnailer->thumbnail_pixmap != None) {
    XFreePixmap (display, thumbnailer->thumbnail_pixmap);
    thumbnailer->thumbnail_pixmap = None;
  }
  if (thumbnailer->window_picture != None) {
    XRenderFreePicture (display, thumbnailer->window_picture);
    thumbnailer->window_picture = None;
  }

  g_free (thumbnailer);
}

#endif  // #ifdef HAVE_XCOMPOSITE

