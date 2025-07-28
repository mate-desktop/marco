#!/usr/bin/bash

# Use grouped output messages
infobegin() {
	echo "::group::${1}"
}
infoend() {
	echo "::endgroup::"
}

# Required packages on Debian
requires=(
	ccache # Use ccache to speed up build
	meson  # Used for meson build
)

# https://salsa.debian.org/debian-mate-team/marco/-/blob/master/debian/control
requires+=(
	autoconf-archive
	autopoint
	gcc
	git
	intltool
	libcanberra-gtk3-dev
	libglib2.0-dev
	libgtk-3-dev
	libgtop2-dev
	libice-dev
	libmate-desktop-dev
	libpango1.0-dev
	libsm-dev
	libstartup-notification0-dev
	libx11-dev
	libxcomposite-dev
	libxcursor-dev
	libxdamage-dev
	libxext-dev
	libxfixes-dev
	libxinerama-dev
	libxpresent-dev
	libxrandr-dev
	libxrender-dev
	libxres-dev
	libxt-dev
	make
	mate-common
	x11proto-present-dev
	yelp-tools
	zenity
)

infobegin "Update system"
apt-get update -qq
infoend

infobegin "Install dependency packages"
env DEBIAN_FRONTEND=noninteractive \
	apt-get install --assume-yes \
	${requires[@]}
infoend
