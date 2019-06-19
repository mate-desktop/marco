cargs = ['-DMARCO_ICON_DIR="marco"']

install_data('marco-window-demo.png',
  install_dir: join_paths(pkgdatadir, 'icons')
)

executable('marco-message',
  'marco-message.c',
  include_directories : [
    include_directories('.'),
    include_directories('..'),
    ],
  dependencies: marco_message_deps,
  link_with : [
    libmarco
    ],
  c_args : cargs,
  install: true,
)

executable('marco-window-demo',
  'marco-window-demo.c',
  include_directories : [
    include_directories('.'),
    include_directories('..'),
    ],
  dependencies: marco_window_demo_deps,
  link_with : [
    libmarco
    ],
  c_args : cargs,
  install: true,
)
