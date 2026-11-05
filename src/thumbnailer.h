// Copyright (c) 2006 Nigel Tao.
// Licenced under the GNU General Public Licence (GPL) version 2.

#ifndef SUPERSWITCHER_THUMBNAILER_H
#define SUPERSWITCHER_THUMBNAILER_H

#include <gtk/gtk.h>
#include <libwnck/libwnck.h>

#include "forward_declarations.h"

#ifdef HAVE_XCOMPOSITE
#include <X11/extensions/Xrender.h>

struct _SSThumbnailer {
  SSWindow *     window;
  WnckWindow *   wnck_window;
  GtkWidget *    drawing_area;

  Pixmap              thumbnail_pixmap;
  cairo_surface_t *   thumbnail_surface;
  Picture             thumbnail_picture;
  Picture             window_picture;
};

SSThumbnailer *   ss_thumbnailer_new    (SSWindow *window, WnckWindow *wnck_window, GtkWidget *drawing_area);
void              ss_thumbnailer_free   (SSThumbnailer *thumbnailer);

gboolean    init_composite     (void);
gboolean    uninit_composite   (void);
#endif

#endif
