AC_PREREQ(2.58)

m4_define([marco_major_version], [1])
m4_define([marco_minor_version], [27])
m4_define([marco_micro_version], [0])

m4_define([marco_version],
          [marco_major_version.marco_minor_version.marco_micro_version])
AC_INIT([marco], [marco_version], [https://github.com/mate-desktop/marco])

AC_CONFIG_SRCDIR(src/core/display.c)
AC_CONFIG_HEADERS(config.h)
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([subdir-objects no-dist-gzip dist-xz check-news])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])
AM_MAINTAINER_MODE
MATE_DEBUG_CHECK([no])
MATE_COMPILE_WARNINGS

dnl **************************************************************************
dnl Library version information
dnl **************************************************************************

m4_define([lt_current], [2])
m4_define([lt_revision], [0])
m4_define([lt_age], [0])
m4_define([lt_version_info], [lt_current:lt_revision:lt_age])

AC_SUBST([LT_VERSION_INFO], [lt_version_info])

AC_CHECK_LIB(m, round)

# Honor aclocal flags
AC_SUBST(ACLOCAL_AMFLAGS, "\${ACLOCAL_FLAGS}")

GETTEXT_PACKAGE=marco
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE",[Name of default gettext domain])

AM_GNU_GETTEXT_VERSION([0.19.8])
AM_GNU_GETTEXT([external])
AC_PROG_CC
AC_HEADER_STDC
AM_PROG_LIBTOOL

#### Integer sizes

AC_CHECK_SIZEOF(char)
AC_CHECK_SIZEOF(short)
AC_CHECK_SIZEOF(long)
AC_CHECK_SIZEOF(int)
AC_CHECK_SIZEOF(void *)
AC_CHECK_SIZEOF(long long)
AC_CHECK_SIZEOF(__int64)

## byte order
AC_C_BIGENDIAN

GIO_MIN_VERSION=2.25.10
GTK_MIN_VERSION=3.22.0

MARCO_PC_MODULES="gtk+-3.0 >= $GTK_MIN_VERSION gio-2.0 >= $GIO_MIN_VERSION pango >= 1.2.0 libcanberra-gtk3 mate-desktop-2.0 >= 1.27.1"

GLIB_GSETTINGS

AC_ARG_ENABLE(verbose-mode,
  AS_HELP_STRING([--disable-verbose-mode],
                 [disable marco's ability to do verbose logging, for embedded/size-sensitive custom builds]),,
  enable_verbose_mode=yes)

if test x$enable_verbose_mode = xyes; then
    AC_DEFINE(WITH_VERBOSE_MODE,1,[Build with verbose mode support])
fi

AC_ARG_ENABLE(sm,
  AS_HELP_STRING([--disable-sm],
                 [disable marco's session management support, for embedded/size-sensitive custom non-MATE builds]),,
  enable_sm=auto)

AC_ARG_ENABLE(startup-notification,
  AS_HELP_STRING([--disable-startup-notification],
                 [disable marco's startup notification support, for embedded/size-sensitive custom non-MATE builds]),,
  enable_startup_notification=auto)

AC_ARG_ENABLE(compositor,
  AS_HELP_STRING([--disable-compositor],
                 [disable marco's compositing manager]),,
  enable_compositor=auto)

AC_ARG_ENABLE(xsync,
  AS_HELP_STRING([--disable-xsync],
                 [disable marco's use of the XSync extension]),,
  enable_xsync=auto)

AC_ARG_ENABLE(render,
  AS_HELP_STRING([--disable-render],
                 [disable marco's use of the RENDER extension]),,
  enable_render=auto)

AC_ARG_ENABLE(shape,
  AS_HELP_STRING([--disable-shape],
                 [disable marco's use of the shaped window extension]),,
  enable_shape=auto)

## try definining HAVE_BACKTRACE
AC_CHECK_HEADERS(execinfo.h, [AC_CHECK_FUNCS(backtrace)])

PKG_CHECK_MODULES(ALL, glib-2.0 >= 2.58.0)
PKG_CHECK_MODULES(MARCO_MESSAGE, gtk+-3.0 >= $GTK_MIN_VERSION)
PKG_CHECK_MODULES(MARCO_WINDOW_DEMO, gtk+-3.0 >= $GTK_MIN_VERSION)

# Unconditionally use this dir to avoid a circular dep with matecc
MATE_KEYBINDINGS_KEYSDIR="${datadir}/mate-control-center/keybindings"
AC_SUBST(MATE_KEYBINDINGS_KEYSDIR)

STARTUP_NOTIFICATION_VERSION=0.7
AC_MSG_CHECKING([Startup notification library >= $STARTUP_NOTIFICATION_VERSION])
if $PKG_CONFIG --atleast-version $STARTUP_NOTIFICATION_VERSION libstartup-notification-1.0; then
   have_startup_notification=yes
else
   have_startup_notification=no
fi
AC_MSG_RESULT($have_startup_notification)

if test x$enable_startup_notification = xyes; then
   have_startup_notification=yes
   echo "startup-notification support forced on"
elif test x$enable_startup_notification = xauto; then
   true
else
   have_startup_notification=no
fi

if test x$have_startup_notification = xyes; then
  echo "Building with libstartup-notification"
  MARCO_PC_MODULES="$MARCO_PC_MODULES libstartup-notification-1.0 >= $STARTUP_NOTIFICATION_VERSION"
  AC_DEFINE(HAVE_STARTUP_NOTIFICATION, , [Building with startup notification support])
else
  echo "Building without libstartup-notification"
fi

## init this, it gets set either in the compositor check below
## or the render-specific check later
have_xrender=no

XCOMPOSITE_VERSION=0.3
if test x$enable_compositor = xyes; then
   have_xcomposite=yes
elif test x$enable_compositor = xauto; then
   echo "Building compositing manager by default now."
   have_xcomposite=yes
else
   have_xcomposite=no
fi

if test x$have_xcomposite = xyes; then
   AC_MSG_CHECKING([Xcomposite >= $XCOMPOSITE_VERSION])
   if $PKG_CONFIG --atleast-version $XCOMPOSITE_VERSION xcomposite; then
      AC_MSG_RESULT([yes])
   else
      AC_MSG_ERROR([no. Use --disable-compositor to disable.])
   fi
fi

if test x$have_xcomposite = xyes; then
  MARCO_PC_MODULES="$MARCO_PC_MODULES xcomposite >= $XCOMPOSITE_VERSION xfixes xrender xdamage xres"
  AC_DEFINE(HAVE_COMPOSITE_EXTENSIONS, 1, [Building with compositing manager support])
  echo "Building with compositing manager"

  ## force on render also
  have_xrender=yes
else
  echo "Building without compositing manager"
fi

## if no compositor, still possibly enable render
if test x$have_xcomposite = xno; then
  XRENDER_VERSION=0.0
  AC_MSG_CHECKING([xrender >= $XRENDER_VERSION])
  if $PKG_CONFIG --atleast-version $XRENDER_VERSION xrender; then
     have_xrender=yes
  else
     have_xrender=no
  fi
  AC_MSG_RESULT($have_xrender)

  if test x$enable_render = xyes; then
     have_xrender=yes
     echo "Render support forced on"
  elif test x$enable_render = xauto; then
     true
  else
     have_xrender=no
  fi

  if test x$have_xrender = xyes; then
     echo "Building with Render"
     MARCO_PC_MODULES="$MARCO_PC_MODULES xrender >= $XRENDER_VERSION"
  fi
fi ## have_composite

if test x$have_xrender = xyes; then
  AC_DEFINE(HAVE_RENDER, , [Building with Render extension support])
fi

AC_MSG_CHECKING([Xcursor])
if $PKG_CONFIG xcursor; then
     have_xcursor=yes
  else
     have_xcursor=no
  fi
  AC_MSG_RESULT($have_xcursor)

if test x$have_xcursor = xyes; then
  echo "Building with Xcursor"
  MARCO_PC_MODULES="$MARCO_PC_MODULES xcursor"
  AC_DEFINE(HAVE_XCURSOR, , [Building with Xcursor support])
fi

AC_MSG_CHECKING([libgtop])
if $PKG_CONFIG libgtop-2.0; then
     have_gtop=yes
  else
     have_gtop=no
  fi
  AC_MSG_RESULT($have_gtop)

if test x$have_gtop = xyes; then
  echo "Building with libgtop"
  MARCO_PC_MODULES="$MARCO_PC_MODULES libgtop-2.0"
  AC_DEFINE(HAVE_GTOP, , [Building with libgtop])
fi

PKG_CHECK_MODULES(MARCO, $MARCO_PC_MODULES)

AC_PATH_XTRA

ALL_X_LIBS="$X_LIBS $X_PRE_LIBS -lX11 $X_EXTRA_LIBS"

# Check for Xinerama extension (Solaris impl or Xfree impl)
marco_save_cppflags="$CPPFLAGS"
CPPFLAGS="$CPPFLAGS $X_CFLAGS"

AC_ARG_ENABLE(xinerama,
  AS_HELP_STRING([--disable-xinerama],
                 [disable marco's use of the Xinerama extension]),
  try_xinerama=$enable_xinerama,try_xinerama=yes)

use_solaris_xinerama=no
use_xfree_xinerama=no
if test "${try_xinerama}" != no; then
    case "$host" in
        *-*-solaris*)
            # Check for solaris
            use_solaris_xinerama=yes
            AC_CHECK_LIB(Xext, XineramaGetInfo,
                         use_solaris_xinerama=yes, use_solaris_xinerama=no,
                         $ALL_X_LIBS)
            if test "x$use_solaris_xinerama" = "xyes"; then
                AC_CHECK_HEADER(X11/extensions/xinerama.h,
                                if test -z "`echo $ALL_X_LIBS | grep "\-lXext" 2> /dev/null`"; then
                                    X_EXTRA_LIBS="-lXext $X_EXTRA_LIBS"
                                fi
                                AC_DEFINE(HAVE_SOLARIS_XINERAMA, , [Have Solaris-style Xinerama])
                                AC_DEFINE(HAVE_XINERAMA, , [Have some version of Xinerama]),
                                use_solaris_xinerama=no,
                                [#include <X11/Xlib.h>])
            fi
            AC_MSG_CHECKING(for Xinerama support on Solaris)
            AC_MSG_RESULT($use_solaris_xinerama);
            ;;
        *)
            # Check for XFree
            use_xfree_xinerama=yes
            AC_CHECK_LIB(Xinerama, XineramaQueryExtension,
                [AC_CHECK_HEADER(X11/extensions/Xinerama.h,
                                 X_EXTRA_LIBS="-lXinerama $X_EXTRA_LIBS"
                                 if test -z "`echo $ALL_X_LIBS | grep "\-lXext" 2> /dev/null`"; then
                                     X_EXTRA_LIBS="-lXext $X_EXTRA_LIBS"
                                 fi
                                 AC_DEFINE(HAVE_XFREE_XINERAMA, , [Have XFree86-style Xinerama])
                                 AC_DEFINE(HAVE_XINERAMA,, [Have some version of Xinerama]),
                                 use_xfree_xinerama=no,
                                 [#include <X11/Xlib.h>])],
                use_xfree_xinerama=no, -lXext $ALL_X_LIBS)
            AC_MSG_CHECKING(for Xinerama support on XFree86)
            AC_MSG_RESULT($use_xfree_xinerama);
            ;;
    esac
fi

CPPFLAGS="$marco_save_cppflags"

SHAPE_LIBS=
found_shape=no
AC_CHECK_LIB(Xext, XShapeQueryExtension,
               [AC_CHECK_HEADER(X11/extensions/shape.h,
                                SHAPE_LIBS=-lXext found_shape=yes)],
               , $ALL_X_LIBS)

if test x$enable_shape = xno; then
   found_shape=no
fi

if test x$enable_shape = xyes; then
   if test "$found_shape" = "no"; then
      AC_MSG_ERROR([--enable-shape forced and Shape not found])
      exit 1
   fi
fi

if test "x$found_shape" = "xyes"; then
   AC_DEFINE(HAVE_SHAPE, , [Have the shape extension library])
fi

found_xkb=no
AC_CHECK_LIB(X11, XkbQueryExtension,
               [AC_CHECK_HEADER(X11/XKBlib.h,
                                found_xkb=yes)],
	, $ALL_X_LIBS)

if test "x$found_xkb" = "xyes"; then
   AC_DEFINE(HAVE_XKB, , [Have keyboard extension library])
fi


RANDR_LIBS=
found_randr=no
AC_CHECK_LIB(Xrandr, XRRUpdateConfiguration,
               [AC_CHECK_HEADER(X11/extensions/Xrandr.h,
                                RANDR_LIBS=-lXrandr found_randr=yes,,
				[#include <X11/Xlib.h>])],
               , -lXrender -lXext $ALL_X_LIBS)

if test "x$found_randr" = "xyes"; then
   AC_DEFINE(HAVE_RANDR, , [Have the Xrandr extension library])
fi

XSYNC_LIBS=
found_xsync=no
AC_CHECK_LIB(Xext, XSyncQueryExtension,
               [AC_CHECK_HEADER(X11/extensions/sync.h,
                                found_xsync=yes,,
				[#include <X11/Xlib.h>])],
               , $ALL_X_LIBS)

if test x$enable_xsync = xno; then
   found_xsync=no
fi

if test x$enable_xsync = xyes; then
   if test "$found_xsync" = "no"; then
      AC_MSG_ERROR([--enable-xsync forced and XSync not found])
      exit 1
   fi
fi

if test "x$found_xsync" = "xyes"; then
   XSYNC_LIBS=-lXext
   AC_DEFINE(HAVE_XSYNC, , [Have the Xsync extension library])
fi

XPRESENT_LIBS=
found_xpresent=no
AC_CHECK_LIB(Xpresent, XPresentPixmap,
               [AC_CHECK_HEADER(X11/extensions/Xpresent.h,
                                XPRESENT_LIBS=-lXpresent found_xpresent=yes,,
        [#include <X11/Xlib.h>])],
               , $ALL_X_LIBS)

if test "x$found_xpresent" = "xyes"; then
   AC_DEFINE(HAVE_PRESENT, , [Have the Xpresent extension library])
fi

MARCO_LIBS="$MARCO_LIBS $XSYNC_LIBS $RANDR_LIBS $SHAPE_LIBS $XPRESENT_LIBS $X_LIBS $X_PRE_LIBS -lX11 $X_EXTRA_LIBS -lm"
MARCO_MESSAGE_LIBS="$MARCO_MESSAGE_LIBS $X_LIBS $X_PRE_LIBS -lX11 $X_EXTRA_LIBS"
MARCO_WINDOW_DEMO_LIBS="$MARCO_WINDOW_DEMO_LIBS $X_LIBS $X_PRE_LIBS -lX11 $X_EXTRA_LIBS"
MARCO_PROPS_LIBS="$MARCO_PROPS_LIBS $X_LIBS $X_PRE_LIBS -lX11 $X_EXTRA_LIBS"

found_sm=no
case "$MARCO_LIBS" in
 *-lSM*)
    found_sm=yes
    ;;
 *)
  AC_CHECK_LIB(SM, SmcSaveYourselfDone,
               [AC_CHECK_HEADERS(X11/SM/SMlib.h,
                                MARCO_LIBS="-lSM -lICE $MARCO_LIBS" found_sm=yes)],
               , $MARCO_LIBS)
    ;;
esac

if test x$enable_sm = xno; then
   found_sm=no
fi

if test x$enable_sm = xyes; then
   if test "$found_sm" = "no"; then
      AC_MSG_ERROR([--enable-sm forced and -lSM not found])
      exit 1
   fi
fi

if test "$found_sm" = "yes"; then
   AC_DEFINE(HAVE_SM, , [Building with SM support])
fi

AM_CONDITIONAL(HAVE_SM, test "$found_sm" = "yes")

HOST_ALIAS=$host_alias
AC_SUBST(HOST_ALIAS)

AC_PATH_PROG(GDK_PIXBUF_CSOURCE, gdk-pixbuf-csource, no)

if test x"$GDK_PIXBUF_CSOURCE" = xno; then
  AC_MSG_ERROR([gdk-pixbuf-csource executable not found in your path - should be installed with GTK])
fi

AC_SUBST(GDK_PIXBUF_CSOURCE)

AC_PATH_PROG(ZENITY, zenity, no)
if test x"$ZENITY" = xno; then
  AC_MSG_ERROR([zenity not found in your path - needed for dialogs])
fi

# Use yelp-tools:
YELP_HELP_INIT

AC_CONFIG_FILES([
Makefile
doc/Makefile
doc/creating_themes/Makefile
doc/man/Makefile
src/Makefile
src/wm-tester/Makefile
src/libmarco-private.pc
src/tools/Makefile
src/themes/Makefile
po/Makefile.in
])

AC_OUTPUT

if test x$enable_verbose_mode = xno; then
	echo "*** WARNING WARNING WARNING WARNING WARNING"
	echo "*** Building without verbose mode"
	echo "*** This means there's no way to debug marco problems."
	echo "*** Please build normal desktop versions of marco"
	echo "*** with verbose mode enabled so users can use it when they report bugs."
fi

dnl ==========================================================================
echo "
marco-$VERSION:

	prefix:                   ${prefix}
	source code location:     ${srcdir}
	compiler:                 ${CC}
        compiler flags:           ${CFLAGS}
        compiler warnings:        ${WARN_CFLAGS}

	XFree86 Xinerama:         ${use_xfree_xinerama}
	Solaris Xinerama:         ${use_solaris_xinerama}
	Startup notification:     ${have_startup_notification}
	Compositing manager:      ${have_xcomposite}
	Session management:       ${found_sm}
	Shape extension:          ${found_shape}
	Resize-and-rotate:        ${found_randr}
	Xsync:                    ${found_xsync}
	Xpresent:                 ${found_xpresent}
	Render:                   ${have_xrender}
	Xcursor:                  ${have_xcursor}
	Verbose logging:          ${enable_verbose_mode}
	Debug:                    ${ax_enable_debug}
"
