themes = [
  'ClearlooksRe',
  'eOS',
  'DustBlue',
  'WinMe',
  'Splint-Left',
  'Dopple',
  'Spidey-Left',
  'Splint',
  'Dopple-Left',
  'Spidey',
]

themedir = join_paths(datadir, 'themes')
foreach theme: themes
  install_subdir(theme,
    install_dir : join_paths(themedir, theme, 'metacity-1'),
    strip_directory : true
  )
endforeach
