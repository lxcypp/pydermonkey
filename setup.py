#! /usr/bin/env python

import os
import sys

if __name__ == '__main__':
    # This code is run if we're executed directly from the command-line.

    myfile = os.path.abspath(__file__)
    mydir = os.path.dirname(myfile)
    sys.path.insert(0, os.path.join(mydir, 'python-modules'))

    args = sys.argv[1:]
    if not args:
        args = ['help']

    # Have paver run this very file as its pavement script.
    args = ['-f', myfile] + args

    import paver.tasks
    paver.tasks.main(args)
    sys.exit(0)

# This code is run if we're executed as a pavement script by paver.

import os
import subprocess
import shutil
import sys
import webbrowser
import urllib

from paver.easy import *
from paver.setuputils import setup
from distutils.core import Extension

SOURCE_FILES = ['pymonkey.cpp',
                'utils.cpp',
                'object.cpp',
                'function.cpp',
                'undefined.cpp',
                'context.cpp',
                'runtime.cpp']

SPIDERMONKEY_DIR = os.path.abspath(os.path.join('spidermonkey', 'obj'))

if not os.path.exists(SPIDERMONKEY_DIR):
    print('WARNING: Spidermonkey objdir not found at %s.' % SPIDERMONKEY_DIR)
    print('Some build tasks may not run properly.\n')

setup_options = dict(
    name='pymonkey',
    version='0.0.1',
    description='Access SpiderMonkey from Python',
    author='Atul Varma',
    author_email='atul@mozilla.com',
    url='http://www.toolness.com'
    )

ext_options = dict(
    include_dirs = [os.path.join(SPIDERMONKEY_DIR, 'dist', 'include')],
    library_dirs = [SPIDERMONKEY_DIR]
    )

if sys.platform == 'win32':
    # MSVC can't find the js_static.lib SpiderMonkey library, even though
    # it exists and distutils is trying to tell it to link to it, so
    # we'll just link to the DLL on Windows platforms and install
    # it in a place where Windows can find it at runtime.
    ext_options['libraries'] = ['js3250']
    ext_options['define_macros'] = [('XP_WIN', 1)]
    # TODO: This is almost certainly not the ideal way to distribute
    # a DLL used by a C extension module.
    setup_options['data_files'] = [
        ('Lib\\site-packages', [os.path.join(SPIDERMONKEY_DIR,
                                             'js3250.dll')])
        ]
else:
    ext_options['libraries'] = ['js_static']

setup_options['ext_modules'] = [Extension('pymonkey',
                                          SOURCE_FILES,
                                          **ext_options)]

setup(**setup_options)

@task
def docs(options):
    """Open the Pymonkey documentation in your web browser."""

    url = os.path.abspath(os.path.join("docs", "rendered", "index.html"))
    url = urllib.pathname2url(url)
    webbrowser.open(url)

@task
def build_docs(options):
    """Build the Pymonkey documentation (requires Sphinx)."""

    retval = subprocess.call(["sphinx-build",
                              "-b", "html",
                              os.path.join("docs", "src"),
                              os.path.join("docs", "rendered")])
    if retval:
        sys.exit(retval)

@task
def test(options):
    """Test the Pymonkey Python C extension."""

    print "Running test suite."

    new_env = {}
    new_env.update(os.environ)

    result = subprocess.call(
        [sys.executable,
         "test_pymonkey.py"],
        env = new_env
        )

    if result:
        sys.exit(result)

    print "Running doctests."

    # We have to add our current directory to the python path so that
    # our doctests can find the pymonkey module.
    new_env['PYTHONPATH'] = os.path.abspath('.')
    retval = subprocess.call(["sphinx-build",
                              "-b", "doctest",
                              os.path.join("docs", "src"),
                              "_doctest_output"],
                             env = new_env)
    if retval:
        sys.exit(retval)
