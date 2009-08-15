import os
import sys
from distutils.core import setup, Extension

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

INCLUDE_DIRS = [os.path.join(SPIDERMONKEY_DIR, 'dist', 'include')]
LIB_DIRS = [os.path.join(SPIDERMONKEY_DIR)]

setup(name='pymonkey',
      version='0.0.1',
      description='Access SpiderMonkey from Python',
      author='Atul Varma',
      author_email='atul@mozilla.com',
      url='http://www.toolness.com',
      ext_modules=[Extension('pymonkey',
                             SOURCE_FILES,
                             include_dirs = INCLUDE_DIRS,
                             library_dirs = LIB_DIRS,
                             libraries = ['js_static'])]
     )
