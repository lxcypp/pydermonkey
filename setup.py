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

import subprocess
import shutil
import errno
import webbrowser
import urllib
import urllib2
import StringIO
import tarfile
import distutils.dir_util
import distutils.core

from paver.easy import *
from paver.setuputils import setup

SOURCE_FILES = ['pydermonkey.cpp',
                'utils.cpp',
                'object.cpp',
                'function.cpp',
                'script.cpp',
                'undefined.cpp',
                'context.cpp',
                'runtime.cpp']

SPIDERMONKEY_TAG = "1.8.1pre"

SPIDERMONKEY_SRC_URL = ("http://hg.toolness.com/spidermonkey/archive/"
                        "%s.tar.bz2" % SPIDERMONKEY_TAG)

SPIDERMONKEY_DIR = os.path.abspath('spidermonkey-%s' % SPIDERMONKEY_TAG)

BUILD_DIR = os.path.abspath('build')

SPIDERMONKEY_OBJDIR = os.path.join(BUILD_DIR, 'spidermonkey')

SPIDERMONKEY_MAKEFILE = os.path.join(SPIDERMONKEY_OBJDIR, 'Makefile')

DOCTEST_OUTPUT_DIR = os.path.join(BUILD_DIR, 'doctest_output')

setup_options = dict(
    name='pydermonkey',
    version='0.0.1',
    description='Access SpiderMonkey from Python',
    author='Atul Varma',
    author_email='atul@mozilla.com',
    url='http://www.toolness.com'
    )

ext_options = dict(
    include_dirs = [os.path.join(SPIDERMONKEY_OBJDIR, 'dist', 'include')],
    library_dirs = [SPIDERMONKEY_OBJDIR]
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
        ('Lib\\site-packages', [os.path.join(SPIDERMONKEY_OBJDIR,
                                             'js3250.dll')])
        ]
else:
    ext_options['libraries'] = ['js_static']

setup_options['ext_modules'] = [
    distutils.core.Extension('pydermonkey',
                             [os.path.join("src", filename)
                              for filename in SOURCE_FILES],
                             **ext_options)
    ]

setup(**setup_options)

@task
def docs(options):
    """Open the Pydermonkey documentation in your web browser."""

    url = os.path.abspath(os.path.join("docs", "rendered", "index.html"))
    url = urllib.pathname2url(url)
    webbrowser.open(url)

@task
def build_spidermonkey(options):
    """Fetch and build SpiderMonkey."""

    if not os.path.exists(SPIDERMONKEY_DIR):
        print("SpiderMonkey source directory not found, "
              "fetching from %s." % SPIDERMONKEY_SRC_URL)

        urlfile = urllib2.urlopen(SPIDERMONKEY_SRC_URL)
        output = StringIO.StringIO()
        done = False
        while not done:
            stuff = urlfile.read(65536)
            if stuff:
                output.write(stuff)
                sys.stdout.write(".")
                sys.stdout.flush()
            else:
                done = True
        sys.stdout.write("\n")
        urlfile.close()
        output.seek(0)

        print "Extracting files."

        tar = tarfile.open("", fileobj = output, mode = "r:bz2")
        tar.extractall()
        output.close()

    distutils.dir_util.mkpath(SPIDERMONKEY_OBJDIR)
    
    if not os.path.exists(SPIDERMONKEY_MAKEFILE):
        print "Running configure."

        configure = os.path.join(SPIDERMONKEY_DIR, "js", "src",
                                 "configure")
        cmdline = [configure,
                   "--enable-static",
                   "--disable-tests"]
        if options.get("build") and options.build.get("debug"):
            cmdline.extend(["--enable-debug",
                            "--enable-gczeal"])
        retval = subprocess.call(cmdline,
                                 cwd = SPIDERMONKEY_OBJDIR)
        if retval:
            sys.exit(retval)

    print "Running make."

    retval = subprocess.call(["make"], cwd = SPIDERMONKEY_OBJDIR)
    if retval:
        sys.exit(retval)

@task
@needs('build_spidermonkey', 'setuptools.command.build')
def build(options):
    """Builds the pydermonkey extension."""

    pass

@task
def build_docs(options):
    """Build the Pydermonkey documentation (requires Sphinx)."""

    retval = subprocess.call(["sphinx-build",
                              "-b", "html",
                              os.path.join("docs", "src"),
                              os.path.join("docs", "rendered")])
    if retval:
        sys.exit(retval)

def maybe_clean(dirname, only_if_empty = False):
    if os.path.exists(dirname):
        if not (only_if_empty and os.listdir(dirname)):
            distutils.dir_util.remove_tree(dirname)

@task
@needs('setuptools.command.clean')
def clean(options):
    """Clean up intermediate files, and optionally other files too."""

    maybe_clean(DOCTEST_OUTPUT_DIR)

    if options.clean.get("all"):
        maybe_clean(SPIDERMONKEY_OBJDIR)

    maybe_clean(BUILD_DIR, only_if_empty = True)

def get_lib_dir():
    # This is really weird and hacky; it ought to be much easier
    # to figure out the default directory that distutils builds
    # its C extension modules in.
    return [os.path.join(BUILD_DIR, name)
            for name in os.listdir(BUILD_DIR)
            if name.startswith("lib.")][0]

@task
def test(options):
    """Test the Pydermonkey Python C extension."""

    print "Running test suite."

    new_env = {}
    new_env.update(os.environ)

    def append_path(env_var, path):
        paths = new_env.get(env_var, '').split(os.path.pathsep)
        paths.append(path)
        new_env[env_var] = os.path.pathsep.join(paths)

    # We have to add our build directory to the python path so that
    # our tests can find the pydermonkey module.
    append_path('PYTHONPATH', get_lib_dir())

    if sys.platform == 'win32':
        # If we're on Windows, ensure that the SpiderMonkey DLL
        # can be loaded.
        append_path('PATH', SPIDERMONKEY_OBJDIR)

    result = subprocess.call(
        [sys.executable,
         os.path.join("test", "test_pydermonkey.py")],
        env = new_env
        )

    if result:
        sys.exit(result)

    print "Running doctests."

    try:
        retval = subprocess.call(["sphinx-build",
                                  "-b", "doctest",
                                  os.path.join("docs", "src"),
                                  DOCTEST_OUTPUT_DIR],
                                 env = new_env)
    except OSError, e:
        if e.errno == errno.ENOENT:
            print "Sphinx not found, skipping doctests."
            retval = 0
        else:
            raise

    if retval:
        sys.exit(retval)
