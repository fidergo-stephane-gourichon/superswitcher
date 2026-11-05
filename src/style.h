// Copyright (c) 2026 Stéphane Gourichon. Written with Claude Code (Anthropic).
// Licenced under the GNU General Public Licence (GPL) version 2.

#ifndef SUPERSWITCHER_STYLE_H
#define SUPERSWITCHER_STYLE_H

#include <gtk/gtk.h>

#define SS_STYLE_CLASS_POPUP          "superswitcher-popup"
#define SS_STYLE_CLASS_WINDOW_ROW     "superswitcher-window-row"
#define SS_STYLE_CLASS_ACTIVE_WINDOW  "superswitcher-active-window"

// Names of the colors defined by ss_style_init, for ss_style_get_color.
#define SS_COLOR_WORKSPACE                "superswitcher_workspace"
#define SS_COLOR_WORKSPACE_BORDER         "superswitcher_workspace_border"
#define SS_COLOR_ACTIVE_WORKSPACE         "superswitcher_active_workspace"
#define SS_COLOR_ACTIVE_WORKSPACE_BORDER  "superswitcher_active_workspace_border"
#define SS_COLOR_WINDOW                   "superswitcher_window"
#define SS_COLOR_WINDOW_BORDER            "superswitcher_window_border"
#define SS_COLOR_ACTIVE_WINDOW            "superswitcher_active_window"
#define SS_COLOR_ACTIVE_WINDOW_BORDER     "superswitcher_active_window_border"
#define SS_COLOR_DRAG_OUTLINE             "superswitcher_drag_outline"

void   ss_style_init        (void);
void   ss_style_get_color   (GtkWidget *widget, const char *name, GdkRGBA *color);

#endif
