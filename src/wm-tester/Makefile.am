
INCLUDES=@MARCO_CFLAGS@

wm_tester_SOURCES= 				\
	main.c

test_gravity_SOURCES=				\
	test-gravity.c

focus_window_SOURCES=				\
	focus-window.c

test_resizing_SOURCES=				\
	test-resizing.c

test_size_hints_SOURCES=			\
	test-size-hints.c

noinst_PROGRAMS=wm-tester test-gravity test-resizing focus-window test-size-hints

wm_tester_LDADD= @MARCO_LIBS@
test_gravity_LDADD= @MARCO_LIBS@
test_resizing_LDADD= @MARCO_LIBS@
test_size_hints_LDADD= @MARCO_LIBS@
focus_window_LDADD= @MARCO_LIBS@