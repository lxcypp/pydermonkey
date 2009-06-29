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

from paver.easy import *

@task
@cmdopts([("objdir=", "o", "The root of your Mozilla objdir")])
def build(options):
    """Build the pymonkey Python C extension."""

    objdir = options.get("objdir")
    if not objdir:
        print("Objdir not specified! Please specify one with "
              "the --objdir option.")
        sys.exit(1)
    objdir = os.path.abspath(objdir)
    incdir = os.path.join(objdir, "dist", "include")
    libdir = os.path.join(objdir, "dist", "lib")

    print "Building extension."

    result = subprocess.call(
        ["g++",
         "-framework", "Python",
         "-I%s" % incdir,
         "-L%s" % libdir,
         "-lmozjs",
         "-o", "pymonkey.so",
         "-dynamiclib",
         "pymonkey.c",
         "utils.c",
         "object.c",
         "undefined.c",
         "context.c",
         "runtime.c"]
        )

    if result:
        sys.exit(result)

    print "Running test suite."

    new_env = {}
    new_env.update(os.environ)
    new_env['DYLD_LIBRARY_PATH'] = libdir

    result = subprocess.call(
        [sys.executable,
         "test_pymonkey.py"],
        env = new_env
        )

    if result:
        sys.exit(result)
