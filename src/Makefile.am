# The main binary
bin_PROGRAMS = superswitcher

superswitcher_SOURCES = \
  dbus-object.c \
  dbus-object.h \
  dbus-server-bindings.h \
  draganddrop.c \
  draganddrop.h \
  forward_declarations.h \
  popup.c \
  popup.h \
  screen.c \
  screen.h \
  superswitcher.c \
  thumbnailer.c \
  thumbnailer.h \
  window.c \
  window.h \
  workspace.c \
  workspace.h \
  xinerama.c \
  xinerama.h

AM_CPPFLAGS = \
  $(SUPERSWITCHER_CFLAGS) \
  -DLOCALEDIR=\""$(datadir)/locale"\" \
  -DSYSCONFDIR=\""$(sysconfdir)"\" \
  -DDATADIR=\""$(datadir)"\" \
  -DG_DISABLE_DEPRECATED \
  -DGTK_DISABLE_DEPRECATED \
  -DWNCK_DISABLE_DEPRECATED \
  -DWNCK_I_KNOW_THIS_IS_UNSTABLE

AM_CFLAGS = @WARN_CFLAGS@

LDADD = ${SUPERSWITCHER_LIBS}


# Note that dbus-server-bindings.h was generated manually:
# dbus-binding-tool --prefix=superswitcher --mode=glib-server dbus.xml > dbus-server-bindings.h
# Really, it could be a Make rule, but for a one-off, we can live without the
# dependency on dbus-binding-tool.


DISTCLEANFILES = \
	Makefile.in
