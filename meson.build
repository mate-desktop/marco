project(
  'marco', 'c',
  version: '1.23.2',
  meson_version: '>=0.47.0',
  license: 'GPL2',
  default_options : [ 'warning_level=1', 'buildtype=debugoptimized', 'c_std=gnu11'],
)

package_name = meson.project_name()
package_version = meson.project_version()

# Versionning
version_arr = package_version.split('.')
version_major = version_arr[0].to_int()
version_minor = version_arr[1].to_int()
version_micro = version_arr[2].to_int()

soversion = 2

if version_minor.is_odd()
  interface_age = 0
else
  interface_age = version_micro
endif

current = version_minor * 100 + version_micro - interface_age
revision = interface_age
libversion = '@0@.@1@.@2@'.format(soversion, current, revision)
package_string = '@0@-@1@.0'.format(package_name, soversion)

config_h = configuration_data()
config_h.set_quoted('PACKAGE', package_name)
config_h.set_quoted('PACKAGE_NAME', package_name)
config_h.set_quoted('PACKAGE_STRING', package_string)
config_h.set_quoted('PACKAGE_VERSION', package_version)
config_h.set_quoted('GETTEXT_PACKAGE', package_name)
config_h.set_quoted('VERSION', package_version)

# Compiler & Project arguments
add_project_arguments([
  '-DHAVE_CONFIG_H',
  '-I' + meson.build_root(),
  ], language: 'c')

# get suported warning flags
test_args = [
  #'-fstack-protector-strong',
  #'-Waggregate-return',
  #'-Warray-bounds',
  '-Wcast-align',
  '-Wchar-subscripts',
  #'-Wclobbered',
  #'-Wdeclaration-after-statement',
  #'-Wempty-body',
  #'-Wextra',
  #'-Wformat=2',
  #'-Wformat-nonliteral',
  #'-Wformat-security',
  ['-Werror=format-security', '-Werror=format=2'],
  #'-Wformat-signedness',
  #'-Wignored-qualifiers',
  #'-Wimplicit-function-declaration',
  #'-Winit-self',
  #'-Winline',
  '-Wmissing-declarations',
  #'-Wmissing-format-attribute',
  #'-Wmissing-include-dirs',
  #'-Wmissing-noreturn',
  #'-Wmissing-parameter-type',
  '-Wmissing-prototypes',
  '-Wnested-externs',
  #'-Wno-discarded-qualifiers',
  #'-Wno-missing-field-initializers',
  #'-Wno-strict-aliasing',
  #'-Wno-suggest-attribute=format',
  #'-Wno-unused-parameter',
  #'-Wold-style-definition',
  #'-Woverride-init',
  #'-Wpacked',
  '-Wpointer-arith',
  #'-Wredundant-decls',
  #'-Wreturn-type',
  '-Wshadow',
  '-Wsign-compare',
  #'-Wstrict-aliasing',
  #'-Wstrict-prototypes',
  #'-Wswitch-default',
  #'-Wtype-limits',
  #'-Wundef',
  #'-Wuninitialized',
  #'-Wunused-but-set-variable',
  #'-Wwrite-strings'
  '-Wall',
  #'-ansi',
  ]

cc = meson.get_compiler('c')

foreach arg: test_args
  if cc.has_multi_arguments(arg)
    add_project_arguments(arg, language : 'c')
  endif
endforeach

# Integer sizes
config_h.set('SIZEOF_CHAR', cc.sizeof('char'))
config_h.set('SIZEOF_SHORT', cc.sizeof('short'))
config_h.set('SIZEOF_LONG', cc.sizeof('long'))
config_h.set('SIZEOF_INT', cc.sizeof('int'))
config_h.set('SIZEOF_VOID_P', cc.sizeof('void *'))
config_h.set('SIZEOF_LONG_LONG', cc.sizeof('long long'))
config_h.set('SIZEOF___INT64', cc.sizeof('__int64'))

# byte order
if build_machine.endian() == 'big'
  config_h.set('WORDS_BIGENDIAN', 1)
endif

gnome = import('gnome')
i18n = import('i18n')
pkg = import('pkgconfig')

# Paths
rootdir = include_directories('.')

srcdir = meson.current_source_dir()
builddir = meson.current_build_dir()

prefix = get_option('prefix')

buildtype = get_option('buildtype')

bindir = join_paths(prefix, get_option('bindir'))
includedir = join_paths(prefix, get_option('includedir'))
libdir = join_paths(prefix, get_option('libdir'))
libexecdir = join_paths(prefix, get_option('libexecdir'))
datadir = join_paths(prefix, get_option('datadir'))
pkgincludedir = join_paths(includedir, package_string)
pkgconfigdir = join_paths(libdir, 'pkgconfig')
pkglibdir = join_paths(libdir, package_name)
pkgdatadir = join_paths(datadir, package_name)

dbusdir = join_paths(datadir, 'dbus-1')
glibdir = join_paths(datadir, 'glib-2.0')
gtkdocdir = join_paths(datadir, 'gtk-doc')
localdir = join_paths(datadir, 'local')
vapidir = join_paths(datadir, 'vala', 'vapi')

# Dependencies
glib_req = '>= 2.58.0'
gio_req = '>= 2.25.10'
gtk_req = '>= 3.22.0'
pango_req = '>= 1.2.0'
libcanberra_gtk_req = '>= 3.22.0'
startup_notification_req = '>= 0.7'

glib_dep = dependency('glib-2.0', version: glib_req)
gtk_dep = dependency('gtk+-3.0', version: gtk_req)
gio_dep = dependency('gio-2.0', version: gio_req)
pango_dep = dependency('pango', version: pango_req)
libcanberra_gtk_dep = dependency('libcanberra-gtk3')
x11_dep = dependency('x11')

marco_deps = [
  glib_dep,
  gtk_dep,
  gio_dep,
  pango_dep,
  libcanberra_gtk_dep,
]

marco_message_deps = [
  gtk_dep,
  x11_dep,
]
marco_window_demo_deps = [
  gtk_dep,
  x11_dep,
]

xcomposite_req ='>= 0.3'
xrender_req ='>= 0.0'

startup_notification_dep = dependency('libstartup-notification-1.0', version:startup_notification_req, required: false)
xcomposite_dep = dependency('xcomposite', version:xcomposite_req, required: false)
xrender_dep = dependency('xrender', version:xrender_req, required: false)
xcursor_dep = dependency('xcursor', required: false)
libgtop_dep = dependency('libgtop-2.0', required: false)
xrandr_dep = dependency('xrandr', required: false)
xpresent_dep = dependency('xpresent', required: false)
xinerama_dep = dependency('xinerama', required: false)
sm_dep = dependency('sm', required: false)

if get_option('verbose-mode')
  config_h.set('WITH_VERBOSE_MODE', 1, description: 'Build with verbose mode support')
endif

if cc.has_function('backtrace', prefix : '#include <execinfo.h>')
  config_h.set('HAVE_BACKTRACE', 1)
endif

build_startup_notification = false
if get_option('startup-notification') and startup_notification_dep.found()
  build_startup_notification = true
  config_h.set('HAVE_STARTUP_NOTIFICATION', 1, description: 'Building with startup notification support')
  config_h.set('SN_API_NOT_YET_FROZEN', 1)
  marco_deps += [ startup_notification_dep ]
endif

build_xcomposite = false
if get_option('compositor') and xcomposite_dep.found()
  build_xcomposite = true
  marco_deps += [ xcomposite_dep, dependency('xfixes'), xrender_dep, dependency('xdamage')]
  config_h.set('HAVE_COMPOSITE_EXTENSIONS', 1)
endif

build_render = false
if get_option('render') and xrender_dep.found()
  build_render = true
  marco_deps += [xrender_dep]
  config_h.set('HAVE_RENDER', 1, description: 'Building with Render extension support')
endif

build_xcursor = false
if xcursor_dep.found()
  build_xcursor = true
  marco_deps += [xcursor_dep]
  config_h.set('HAVE_XCURSOR', 1, description: 'Building with Render extension support')
endif

if libgtop_dep.found()
  marco_deps += [libgtop_dep]
  config_h.set('HAVE_GTOP', 1, description: 'Building with libgtop')
endif

build_xinerama = false
if get_option('xinerama') and xinerama_dep.found()
  build_xinerama = true
  config_h.set('HAVE_XFREE_XINERAMA', 1)
  config_h.set('HAVE_XINERAMA', 1)
  marco_deps += [xinerama_dep]
endif

#xext
build_shape = false
if cc.has_header('X11/extensions/shape.h')
  build_shape = true
  config_h.set('HAVE_SHAPE', 1)
endif

if cc.has_header('X11/XKBlib.h')
  config_h.set('HAVE_XKB', 1)
endif

build_randr = false
if xrandr_dep.found()
  build_randr = true
  config_h.set('HAVE_RANDR', 1, description: 'Have the Xrandr extension library')
  marco_deps += [ xrandr_dep ]
endif

#xext
build_xsync = false
if get_option('xsync') and cc.has_header('X11/extensions/sync.h')
  build_xsync = true
  config_h.set('HAVE_XSYNC', 1)
endif

build_xpresent = xpresent_dep.found()
if build_xpresent
  config_h.set('HAVE_PRESENT', 1, description: 'Have the Xpresent extension library')
  marco_deps += [ xpresent_dep ]
endif

libm = cc.find_library('m', required: false)
if libm.found()
  marco_deps += [libm]
endif

build_sm = false
ice_dep = dependency('ice', required: false)
if get_option('sm') and sm_dep.found() and ice_dep.found()
  build_sm = true
  config_h.set('HAVE_SM', 1)
  marco_deps += [ sm_dep, ice_dep ]
  marco_message_deps += [ sm_dep, ice_dep ]
endif

gdk_pixbuf_csource = find_program('gdk-pixbuf-csource')
zenity = find_program('zenity')

libxext = cc.find_library('Xext', required: false)
if build_xsync
  marco_deps += [libxext]
endif

config_h.set_quoted('MARCO_LOCALEDIR', localdir)
config_h.set_quoted('LIBDIR', libdir)
config_h.set_quoted('MARCO_DATADIR', datadir)

configure_file(
  output: 'config.h',
  configuration: config_h
)

subdir('doc')
subdir('src')
subdir('po')

summary = [
  '',
  'marco-@0@:'.format(meson.project_version()),
  '',
  '	prefix:                   @0@'.format(prefix),
  '	source code location:     @0@'.format(srcdir),
  '	compiler:                 @0@'.format(cc.get_id()),
  '',
  '	Xinerama:                 @0@'.format(build_xinerama),
  '	Startup notification:     @0@'.format(build_startup_notification),
  '	Compositing manager:      @0@'.format(build_xcomposite),
  '	Session management:       @0@'.format(build_sm),
  '	Shape extension:          @0@'.format(build_shape),
  '	Resize-and-rotate:        @0@'.format(build_randr),
  '	Xsync:                    @0@'.format(build_xsync),
  '	Xpresent:                 @0@'.format(build_xpresent),
  '	Render:                   @0@'.format(build_render),
  '	Xcursor:                  @0@'.format(build_xcursor),
  '',
]

meson.add_install_script(
  'meson_post_install.py',
  libdir, datadir,
)

message('\n'.join(summary))
