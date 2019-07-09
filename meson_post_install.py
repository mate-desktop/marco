#!/usr/bin/env python3

import os
import subprocess
import sys

libdir = sys.argv[1]
datadir = sys.argv[2]

# Packaging tools define DESTDIR and this isn't needed for them
if 'DESTDIR' not in os.environ:
    print('Compiling gsettings schemas...')
    subprocess.call(['glib-compile-schemas',
                     os.path.join(datadir, 'glib-2.0', 'schemas')])

    print('Updating icon cache...')
    subprocess.call(['gtk-update-icon-cache', '-qtf',
                     os.path.join(datadir, 'icons', 'hicolor')])

    print('Updating desktop database...')
    subprocess.call(['update-desktop-database', '-q',
                     os.path.join(datadir, 'applications')])
