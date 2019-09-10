
SUBDIRS=src po doc

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

EXTRA_DIST = autogen.sh HACKING \
	intltool-extract.in intltool-merge.in intltool-update.in

DISTCLEANFILES = intltool-extract intltool-merge intltool-update po/stamp-it po/.intltool-merge-cache

# Build ChangeLog from GIT  history
ChangeLog:
	$(AM_V_GEN) if test -d $(top_srcdir)/.git; then \
		GIT_DIR="$(top_srcdir)/.git" git log --stat > $@; \
	fi

dist: ChangeLog

.PHONY: ChangeLog

-include $(top_srcdir)/git.mk
