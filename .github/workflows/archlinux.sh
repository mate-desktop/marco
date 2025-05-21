#!/usr/bin/bash

# Use grouped output messages
infobegin() {
	echo "::group::${1}"
}
infoend() {
	echo "::endgroup::"
}

# Required packages on Archlinux
requires=(
	ccache # Use ccache to speed up build
	clang  # Build with clang on Archlinux
	meson  # Used for meson build
)

requires+=(
	autoconf-archive
	gcc
	git
	glib2
	gtk3
	intltool
	libcanberra
	libgtop
	libxpresent
	libxres
	make
	mate-common
	mate-desktop
	which
	yelp-tools
	zenity
)

infobegin "Update system"
pacman --noconfirm -Syu
infoend

infobegin "Install dependency packages"
pacman --noconfirm -S ${requires[@]}
infoend
