lib_LTLIBRARIES = libmarco-private.la

SUBDIRS=wm-tester tools themes

INCLUDES=-I$(srcdir)/include -DMARCO_LIBEXECDIR=\"$(libexecdir)\" -DHOST_ALIAS=\"@HOST_ALIAS@\" -DMARCO_LOCALEDIR=\"$(prefix)/@DATADIRNAME@/locale\" -DMARCO_PKGDATADIR=\"$(pkgdatadir)\" -DMARCO_DATADIR=\"$(datadir)\" -DG_LOG_DOMAIN=\"marco\" -DSN_API_NOT_YET_FROZEN=1 @MARCO_CFLAGS@

marco_SOURCES = \
	core/async-getprop.c \
	core/async-getprop.h \
	core/atomnames.h \
	core/bell.c \
	core/bell.h \
	core/boxes.c \
	include/boxes.h \
	compositor/compositor.c \
	compositor/compositor-private.h \
	compositor/compositor-xrender.c \
	compositor/compositor-xrender.h \
	include/compositor.h \
	core/above-tab-keycode.c		\
	core/constraints.c \
	core/constraints.h \
	core/core.c \
	core/delete.c \
	core/display.c \
	core/display-private.h \
	include/display.h \
	ui/draw-workspace.c \
	ui/draw-workspace.h \
	core/edge-resistance.c \
	core/edge-resistance.h \
	core/effects.c \
	core/effects.h \
	core/errors.c \
	include/errors.h \
	core/eventqueue.c \
	core/eventqueue.h \
	core/frame.c \
	core/frame-private.h \
	include/frame.h \
	ui/gradient.c \
	ui/gradient.h \
	core/group-private.h \
	core/group-props.c \
	core/group-props.h \
	core/group.c \
	core/group.h \
	core/iconcache.c \
	core/iconcache.h \
	core/keybindings.c \
	core/keybindings.h \
	core/main.c \
	include/main.h \
	core/marco-Xatomtype.h \
	core/place.c \
	core/place.h \
	core/prefs.c \
	include/prefs.h \
	core/screen.c \
	core/screen-private.h \
	include/screen.h \
	include/types.h \
	core/session.c \
	core/session.h \
	core/stack.c \
	core/stack.h \
	core/util.c \
	include/util.h \
	core/window-props.c \
	core/window-props.h \
	core/window.c \
	core/window-private.h \
	include/window.h \
	core/workspace.c \
	core/workspace.h \
	core/xprops.c \
	include/xprops.h \
	include/common.h \
	include/core.h \
	include/ui.h \
	inlinepixbufs.h				\
	ui/fixedtip.c \
	ui/fixedtip.h \
	ui/frames.c \
	ui/frames.h \
	ui/menu.c \
	ui/menu.h \
	ui/metaaccellabel.c \
	ui/metaaccellabel.h \
	ui/resizepopup.c \
	include/resizepopup.h \
	ui/tabpopup.c \
	include/tabpopup.h \
	ui/theme-parser.c \
	ui/theme-parser.h \
	ui/theme.c \
	ui/theme.h \
	ui/themewidget.c \
	ui/themewidget.h \
	ui/ui.c \
	include/all-keybindings.h

# by setting libmarco_private_la_CFLAGS, the files shared with
# marco proper will be compiled with different names.
libmarco_private_la_CFLAGS =
libmarco_private_la_SOURCES = \
	core/boxes.c \
	include/boxes.h \
	ui/gradient.c \
	ui/gradient.h \
	core/util.c \
	include/util.h \
	include/common.h \
	ui/preview-widget.c \
	ui/preview-widget.h \
	ui/theme-parser.c \
	ui/theme-parser.h \
	ui/theme.c \
	ui/theme.h

libmarco_private_la_LDFLAGS = -no-undefined
libmarco_private_la_LIBADD = @MARCO_LIBS@

libmarcoincludedir = $(includedir)/marco-1/marco-private

libmarcoinclude_HEADERS = \
	include/boxes.h \
	ui/gradient.h \
	include/util.h \
	include/common.h \
	ui/preview-widget.h \
	ui/theme-parser.h \
	ui/theme.h

marco_theme_viewer_SOURCES= \
	ui/theme-viewer.c

schema_bindings_SOURCES = \
	core/schema-bindings.c \
	marco.schemas.in.in

schema_bindings_LDADD = @MARCO_LIBS@
marco.schemas.in: schema_bindings ${srcdir}/marco.schemas.in.in
	@echo Generating keybinding schemas... ${srcdir}/marco.schemas.in.in
	$(AM_V_GEN)${builddir}/schema_bindings ${srcdir}/marco.schemas.in.in ${builddir}/marco.schemas.in

bin_PROGRAMS=marco marco-theme-viewer

EFENCE=
marco_LDADD=@MARCO_LIBS@ $(EFENCE)
marco_theme_viewer_LDADD= @MARCO_LIBS@ libmarco-private.la

testboxes_SOURCES=include/util.h core/util.c include/boxes.h core/boxes.c core/testboxes.c
testgradient_SOURCES=ui/gradient.h ui/gradient.c ui/testgradient.c
testasyncgetprop_SOURCES=core/async-getprop.h core/async-getprop.c core/testasyncgetprop.c

noinst_PROGRAMS=testboxes testgradient testasyncgetprop schema_bindings

testboxes_LDADD= @MARCO_LIBS@
testgradient_LDADD= @MARCO_LIBS@
testasyncgetprop_LDADD= @MARCO_LIBS@

@INTLTOOL_DESKTOP_RULE@

desktopfilesdir=$(datadir)/applications
desktopfiles_in_files=marco.desktop.in
desktopfiles_files=$(desktopfiles_in_files:.desktop.in=.desktop)
desktopfiles_DATA = $(desktopfiles_files)

wmpropertiesdir=$(datadir)/mate/wm-properties
wmproperties_in_files=marco-wm.desktop.in
wmproperties_files=$(wmproperties_in_files:.desktop.in=.desktop)
wmproperties_DATA = $(wmproperties_files)

schemadir = @MATECONF_SCHEMA_FILE_DIR@
schema_in_files = marco.schemas.in
schema_DATA = $(schema_in_files:.schemas.in=.schemas)

@INTLTOOL_XML_NOMERGE_RULE@

xmldir = @MATE_KEYBINDINGS_KEYSDIR@
xml_in_files = 50-marco-desktop-key.xml.in 50-marco-key.xml.in
xml_DATA = $(xml_in_files:.xml.in=.xml)

@INTLTOOL_SCHEMAS_RULE@

if MATECONF_SCHEMAS_INSTALL
install-data-local:
	MATECONF_CONFIG_SOURCE=$(MATECONF_SCHEMA_CONFIG_SOURCE) $(MATECONFTOOL) --makefile-install-rule $(schema_DATA)
else
install-data-local:
endif

IMAGES=stock_maximize.png stock_minimize.png stock_delete.png
VARIABLES=stock_maximize_data $(srcdir)/stock_maximize.png \
          stock_minimize_data $(srcdir)/stock_minimize.png \
          stock_delete_data $(srcdir)/stock_delete.png

BUILT_SOURCES = inlinepixbufs.h
CLEANFILES = inlinepixbufs.h marco.desktop marco-wm.desktop marco.schemas marco.schemas.in 	50-marco-desktop-key.xml 50-marco-key.xml

inlinepixbufs.h: $(IMAGES)
	$(AM_V_GEN)$(GDK_PIXBUF_CSOURCE) --raw --build-list $(VARIABLES) >$(srcdir)/inlinepixbufs.h

pkgconfigdir = $(libdir)/pkgconfig

pkgconfig_DATA = libmarco-private.pc

EXTRA_DIST=$(desktopfiles_files) \
	$(wmproperties_files) \
	$(IMAGES) \
	$(schema_DATA) \
	$(desktopfiles_in_files) \
	$(wmproperties_in_files) \
	$(schema_in_files) \
	$(xml_in_files) \
	libmarco-private.pc.in

