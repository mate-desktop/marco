#!/usr/bin/bash

# Use grouped output messages
infobegin() {
	echo "::group::${1}"
}
infoend() {
	echo "::endgroup::"
}

# Required packages on Fedora
requires=(
	ccache # Use ccache to speed up build
	meson  # Used for meson build
)

# https://src.fedoraproject.org/rpms/marco/blob/rawhide/f/marco.spec
requires+=(
	desktop-file-utils
	gcc
	gtk3-devel
	libSM-devel
	libXdamage-devel
	libXpresent-devel
	libXres-devel
	libcanberra-devel
	libgtop2-devel
	libsoup-devel
	make
	mate-common
	mate-desktop-devel
	redhat-rpm-config
	startup-notification-devel
	yelp-tools
	zenity
)

infobegin "Update system"
dnf update -y
infoend

infobegin "Install dependency packages"
dnf install -y ${requires[@]}
infoend
