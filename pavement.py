import os
import subprocess
import shutil
import sys

from paver.easy import *

@task
def auto(options):
    objdir = os.path.join("..", "mozilla-stuff", "basic-firefox")
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
