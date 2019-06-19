test1 = executable('wm-tester',
  'main.c',
  include_directories : [
    include_directories('.'),
    include_directories('..'),
    ],
  dependencies: marco_deps,
  link_with : [
    libmarco
    ],
)

test2 = executable('test-gravity',
  'test-gravity.c',
  include_directories : [
    include_directories('.'),
    include_directories('..'),
    ],
  dependencies: marco_deps,
  link_with : [
    libmarco
    ],
)

test3 = executable('test-resizing',
  'test-resizing.c',
  include_directories : [
    include_directories('.'),
    include_directories('..'),
    ],
  dependencies: marco_deps,
  link_with : [
    libmarco
    ],
)

test4  = executable('focus-window',
  'focus-window.c',
  include_directories : [
    include_directories('.'),
    include_directories('..'),
    ],
  dependencies: marco_deps,
  link_with : [
    libmarco
    ],
)

test5 = executable('test-size-hints',
  'test-size-hints.c',
  include_directories : [
    include_directories('.'),
    include_directories('..'),
    ],
  dependencies: marco_deps,
  link_with : [
    libmarco
    ],
)

test('wm-tester', test1)
test('test-gravity',  test2)
test('test-resizing', test3)
test('focus-window',  test4)
test('test-size-hints',  test5)
