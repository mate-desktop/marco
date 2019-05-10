icondir=$(pkgdatadir)/icons
icon_DATA=marco-window-demo.png

AM_CPPFLAGS=@MARCO_WINDOW_DEMO_CFLAGS@ @MARCO_MESSAGE_CFLAGS@ \
	-DMARCO_ICON_DIR=\"$(pkgdatadir)/icons\" \
	-DMARCO_LOCALEDIR=\"$(prefix)/@DATADIRNAME@/locale\"

marco_message_SOURCES= 				\
	marco-message.c

marco_window_demo_SOURCES=				\
	marco-window-demo.c

bin_PROGRAMS=marco-message marco-window-demo

marco_message_LDADD= @MARCO_MESSAGE_LIBS@
marco_window_demo_LDADD= @MARCO_WINDOW_DEMO_LIBS@

EXTRA_DIST=$(icon_DATA)


-include $(top_srcdir)/git.mk
