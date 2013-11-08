@INTLTOOL_DESKTOP_RULE@

icondir=$(pkgdatadir)/icons
icon_DATA=marco-window-demo.png

AM_CPPFLAGS=@MARCO_WINDOW_DEMO_CFLAGS@ @MARCO_MESSAGE_CFLAGS@ \
	-DMARCO_ICON_DIR=\"$(pkgdatadir)/icons\" \
	-DMARCO_LOCALEDIR=\"$(prefix)/@DATADIRNAME@/locale\"

marco_message_SOURCES= 				\
	marco-message.c

marco_window_demo_SOURCES=				\
	marco-window-demo.c

marco_mag_SOURCES=					\
	marco-mag.c

marco_grayscale_SOURCES=				\
	marco-grayscale.c

bin_PROGRAMS=marco-message marco-window-demo

## cheesy hacks I use, don't really have any business existing. ;-)
noinst_PROGRAMS=marco-mag marco-grayscale

marco_message_LDADD= @MARCO_MESSAGE_LIBS@
marco_window_demo_LDADD= @MARCO_WINDOW_DEMO_LIBS@
marco_mag_LDADD= @MARCO_WINDOW_DEMO_LIBS@ -lm
marco_grayscale_LDADD = @MARCO_WINDOW_DEMO_LIBS@

EXTRA_DIST=$(icon_DATA)

