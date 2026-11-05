// Copyright (c) 2006 Nigel Tao.
// Licenced under the GNU General Public Licence (GPL) version 2.

#ifndef SUPERSWITCHER_FORWARD_DECLARATIONS_H
#define SUPERSWITCHER_FORWARD_DECLARATIONS_H

typedef struct _SSDragAndDrop    SSDragAndDrop;
typedef struct _SSScreen         SSScreen;
typedef struct _SSWindow         SSWindow;
typedef struct _SSWorkspace      SSWorkspace;
typedef struct _SSXinerama       SSXinerama;
typedef struct _SSXineramaScreen SSXineramaScreen;

#define MAX_REASONABLE_WORKSPACES  36
#define WINDOW_ROW_SPACING         6
#define WORKSPACE_COLUMN_SPACING   6

#define ABNORMAL_EXIT_CODE_ANOTHER_INSTANCE_IS_RUNNING  1
#define ABNORMAL_EXIT_CODE_UNKNOWN_COMMAND_LINE_OPTION  2
#define ABNORMAL_EXIT_CODE_WAYLAND_SESSION              3

//#include <glib/gerror.h>
//#include <glib/gtypes.h>
#include <glib.h>
gboolean   superswitcher_hide_popup     (void *, GError **);
gboolean   superswitcher_show_popup     (void *, GError **);
gboolean   superswitcher_toggle_popup   (void *, GError **);

#ifdef HAVE_XCOMPOSITE
extern gboolean show_window_thumbnails;
typedef struct _SSThumbnailer SSThumbnailer;
#endif

extern gboolean window_manager_uses_viewports;

#endif
