
SUBDIRS = po src doc

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

EXTRA_DIST = \
	its \
	autogen.sh \
	HACKING \
	meson.build \
	meson_options.txt \
	meson_post_install.py \
	po/meson.build

DISTCLEANFILES = po/stamp-it

# Build ChangeLog from GIT  history
ChangeLog:
	$(AM_V_GEN) if test -d $(top_srcdir)/.git; then \
		GIT_DIR="$(top_srcdir)/.git" git log --stat > $@; \
	fi

dist: ChangeLog

.PHONY: ChangeLog

-include $(top_srcdir)/git.mk
