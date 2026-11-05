# Initial blurb
AC_PREREQ(2.52)
AC_INIT(superswitcher, 0.9.3, https://github.com/fidergo-stephane-gourichon/superswitcher)
AC_CONFIG_SRCDIR(src/superswitcher.c)
AM_INIT_AUTOMAKE(AC_PACKAGE_NAME, AC_PACKAGE_VERSION)

# Honor aclocal flags
ACLOCAL="$ACLOCAL $ACLOCAL_FLAGS"

# Do the gettext/i18n stuff
GETTEXT_PACKAGE=superswitcher
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE",
  [The name of the gettext package.])
AC_SUBST(GETTEXT_PACKAGE)

AM_MAINTAINER_MODE

AC_ISC_POSIX
AC_PROG_CC
AM_PROG_CC_STDC
AC_HEADER_STDC
GNOME_COMPILE_WARNINGS(yes)

AC_PROG_INTLTOOL([0.20])
ALL_LINGUAS=""
AM_GLIB_GNU_GETTEXT


PKG_CHECK_MODULES(SUPERSWITCHER,
  glib-2.0
  gdk-2.0
  gtk+-2.0 >= 2.12
  libwnck-1.0 >= 2.20
  x11)
AC_SUBST(SUPERSWITCHER_CFLAGS)
AC_SUBST(SUPERSWITCHER_LIBS)


# Older XFree86s don't use pkg-config.  Yuck.
AC_PATH_XTRA
SUPERSWITCHER_CFLAGS="$SUPERSWITCHER_CFLAGS $X_CFLAGS"
SUPERSWITCHER_LIBS="$SUPERSWITCHER_LIBS $X_LIBS"

AC_CHECK_HEADER(X11/extensions/Xinerama.h,
  echo "Building with Xinerama."
  AC_DEFINE(HAVE_XINERAMA, , [If we have Xinerama])
  SUPERSWITCHER_LIBS="$SUPERSWITCHER_LIBS -lXinerama"
,
  echo "Building without Xinerama."
)


# We use gconf to look at metacity's focus-follows-mouse prefs.  It is optional
# because we might want to use superswitcher in a non-GNOME environment
# (metacity and gconf are GNOME specific).
if $PKG_CONFIG --exists gconf-2.0; then
  echo "Building with gconf."
  SUPERSWITCHER_CFLAGS="$SUPERSWITCHER_CFLAGS `$PKG_CONFIG --cflags gconf-2.0`"
  SUPERSWITCHER_LIBS="$SUPERSWITCHER_LIBS `$PKG_CONFIG --libs gconf-2.0`"
  AC_DEFINE(HAVE_GCONF, , [If we have gconf])
else
  echo "Building without gconf."
fi


# D-Bus is similarly optional.
if $PKG_CONFIG --exists dbus-glib-1; then
  echo "Building with dbus-glib."
  SUPERSWITCHER_CFLAGS="$SUPERSWITCHER_CFLAGS `$PKG_CONFIG --cflags dbus-glib-1`"
  SUPERSWITCHER_LIBS="$SUPERSWITCHER_LIBS `$PKG_CONFIG --libs dbus-glib-1`"
  AC_DEFINE(HAVE_DBUS_GLIB, , [If we have dbus-glib])
else
  echo "Building without dbus-glib."
fi


# XComposite, XRender etc. are similarly optional.
if [$PKG_CONFIG --atleast-version 0.2 xcomposite] &&
   [$PKG_CONFIG --atleast-version 0.6 xrender]; then
  echo "Building with xcomposite and friends (xrender, etc.)."
  SUPERSWITCHER_CFLAGS="$SUPERSWITCHER_CFLAGS `$PKG_CONFIG --cflags xcomposite xrender`"
  SUPERSWITCHER_LIBS="$SUPERSWITCHER_LIBS `$PKG_CONFIG --libs xcomposite xrender`"
  AC_DEFINE(HAVE_XCOMPOSITE, , [If we have xcomposite])
else
  echo "Building without xcomposite and friends (xrender, etc.)."
fi


# Blank line to separate the conditional "Building with[out] ..." messages.
echo

AC_OUTPUT([
  Makefile
  data/Makefile
  po/Makefile.in
  src/Makefile
])
