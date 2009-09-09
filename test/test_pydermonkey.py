import gc
import sys
import unittest
import weakref
import time
import threading

import pydermonkey

class PydermonkeyTests(unittest.TestCase):
    def setUp(self):
        self._teardowns = []

    def tearDown(self):
        self.last_exception = None
        while self._teardowns:
            obj = self._teardowns.pop()
            runtime = obj.get_runtime()
            runtime.new_context().clear_object_private(obj)
            del runtime
            del obj
        self.assertEqual(pydermonkey.get_debug_info()['runtime_count'], 0)

    def _clearOnTeardown(self, obj):
        self._teardowns.append(obj)

    def _evaljs(self, code, cx=None, obj=None):
        if cx is None:
            rt = pydermonkey.Runtime()
            cx = rt.new_context()
        if obj is None:
            obj = cx.new_object()
            cx.init_standard_classes(obj)
        return cx.evaluate_script(obj, code, '<string>', 1)

    def _execjs(self, code):
        rt = pydermonkey.Runtime()
        cx = rt.new_context()
        obj = cx.new_object()
        cx.init_standard_classes(obj)
        script = cx.compile_script(code, '<string>', 1)
        return cx.execute_script(obj, script)

    def _evalJsWrappedPyFunc(self, func, code):
        cx = pydermonkey.Runtime().new_context()
        obj = cx.new_object()
        cx.init_standard_classes(obj)
        jsfunc = cx.new_function(func, func.__name__)
        self._clearOnTeardown(jsfunc)
        cx.define_property(obj, func.__name__, jsfunc)
        return cx.evaluate_script(obj, code, '<string>', 1)

    def assertRaises(self, exctype, func, *args):
        was_raised = False
        try:
            func(*args)
        except exctype, e:
            self.last_exception = e
            was_raised = True
        self.assertTrue(was_raised)

    def testVersionIsCorrect(self):
        # Really hackish way of importing values from metadata.py.
        import os

        mydir = os.path.dirname(__file__)
        rootdir = os.path.join(mydir, '..')
        rootdir = os.path.normpath(rootdir)
        sys.path.insert(0, rootdir)

        import metadata

        self.assertEqual(pydermonkey.__version__, metadata.VERSION)

    def testDeletePropertyWorks(self):
        cx = pydermonkey.Runtime().new_context()
        obj = cx.new_object()
        cx.define_property(obj, 'foo', 1)
        self.assertEqual(cx.delete_property(obj, 'foo'), True)
        self.assertEqual(cx.delete_property(obj, 'foo'), True)
        self.assertEqual(cx.get_property(obj, 'foo'),
                         pydermonkey.undefined)

    def testSetPropertyWorks(self):
        cx = pydermonkey.Runtime().new_context()
        obj = cx.new_object()
        self.assertEqual(cx.set_property(obj, 'blah', 5), 5)
        self.assertEqual(cx.set_property(obj, 3, 2), 2)
        self.assertEqual(cx.set_property(obj, u'blah\u2026', 5), 5)
        self.assertEqual(cx.get_property(obj, 3), 2)

    def testSetPropertyReturnsDifferentValue(self):
        cx = pydermonkey.Runtime().new_context()
        obj = cx.new_object()
        cx.init_standard_classes(obj)
        o2 = self._evaljs("({set blah() { return 5; }})", cx, obj)
        self.assertEqual(cx.set_property(o2, 'blah', 3), 5)

    def testSetPropertyRaisesExceptionOnReadOnly(self):
        cx = pydermonkey.Runtime().new_context()
        obj = cx.new_object()
        cx.init_standard_classes(obj)
        o2 = self._evaljs("({get blah() { return 5; }})", cx, obj)
        self.assertRaises(
            pydermonkey.error,
            cx.set_property,
            o2, 'blah', 3
            )
        self.assertEqual(self.last_exception.args[0],
                         "setting a property that has only a getter")

    def testDefinePropertyRaisesNoExceptionOnReadOnly(self):
        cx = pydermonkey.Runtime().new_context()
        obj = cx.new_object()
        cx.init_standard_classes(obj)
        o2 = self._evaljs("({get blah() { return 5; }})", cx, obj)
        cx.define_property(o2, 'blah', 3)
        self.assertEqual(cx.get_property(o2, 'blah'), 3)

    def testLookupPropertyWorks(self):
        cx = pydermonkey.Runtime().new_context()
        obj = cx.new_object()
        cx.init_standard_classes(obj)
        o2 = self._evaljs("({foo: 1, get blah() { return 5; }})", cx, obj)
        self.assertEqual(cx.lookup_property(o2, 'bar'),
                         pydermonkey.undefined)
        self.assertEqual(cx.lookup_property(o2, 'foo'), 1)
        self.assertEqual(cx.lookup_property(o2, 'blah'), True)
        self.assertEqual(cx.get_property(o2, 'blah'), 5)
        self.assertEqual(cx.lookup_property(o2, 'blah'), True)

    def testSyntaxErrorsAreRaised(self):
        for run in [self._evaljs, self._execjs]:
            self.assertRaises(pydermonkey.error, run, '5f')
            self.assertEqual(
                self.last_exception.args[1],
                u'SyntaxError: missing ; before statement'
                )

    def testGetStackOnEmptyStackReturnsNone(self):
        cx = pydermonkey.Runtime().new_context()
        self.assertEqual(cx.get_stack(), None)

    def testGetStackWorks(self):
        stack_holder = []

        def func(cx, this, args):
            stack_holder.append(cx.get_stack())

        cx = pydermonkey.Runtime().new_context()
        obj = cx.new_object()
        cx.init_standard_classes(obj)
        jsfunc = cx.new_function(func, func.__name__)
        self._clearOnTeardown(jsfunc)
        cx.define_property(obj, func.__name__, jsfunc)
        cx.evaluate_script(obj, '(function closure() { \nfunc() })()',
                           '<string>', 1)
        stack = stack_holder[0]
        script = stack['caller']['caller']['script']
        pc = stack['caller']['caller']['pc']
        closure = stack['caller']['function']
        self.assertEqual(closure.name, 'closure')
        self.assertEqual(closure.filename, '<string>')
        self.assertEqual(stack['caller']['script'], None)
        self.assertEqual(stack['caller']['lineno'], 2)
        self.assertEqual(script.filename, '<string>')
        self.assertEqual(stack['caller']['caller']['lineno'], 1)
        self.assertTrue(pc >= 0 and pc < len(buffer(script)))
        self.assertEqual(stack['caller']['caller']['caller'], None)

    def testNewObjectTakesProto(self):
        cx = pydermonkey.Runtime().new_context()
        obj = cx.new_object()
        cx.define_property(obj, 5, "foo")
        obj2 = cx.new_object(None, obj)
        self.assertEqual(cx.get_property(obj2, 5), "foo")

    def testInitStandardClassesWorksTwice(self):
        cx = pydermonkey.Runtime().new_context()
        obj = cx.new_object()
        cx.init_standard_classes(obj)
        obj2 = cx.new_object()

        # TODO: This is really just a workaround for issue #3:
        # http://code.google.com/p/pydermonkey/issues/detail?id=3
        self.assertRaises(
            pydermonkey.error,
            cx.init_standard_classes,
            obj2
            )
        self.assertEqual(
            self.last_exception.args[0],
            "Can't init standard classes on the same context twice."
            )             

    def testNewArrayObjectWorks(self):
        cx = pydermonkey.Runtime().new_context()
        array = cx.new_array_object()
        self.assertTrue(cx.is_array_object(array))

    def testIsArrayWorks(self):
        cx = pydermonkey.Runtime().new_context()
        obj = cx.new_object()
        self.assertFalse(cx.is_array_object(obj))
        array = cx.evaluate_script(obj, '[1]', '<string>', 1)
        self.assertTrue(cx.is_array_object(array))

    def testEnumerateWorks(self):
        cx = pydermonkey.Runtime().new_context()
        obj = cx.new_object()
        cx.evaluate_script(obj, "var blah = 1; var foo = 2; this[0] = 5;",
                           "<string>", 1)
        self.assertEqual(cx.enumerate(obj), ("blah", "foo", 0))

    def testBigArrayIndicesRaiseValueError(self):
        cx = pydermonkey.Runtime().new_context()
        obj = cx.new_object()
        self.assertRaises(
            ValueError,
            cx.define_property,
            obj, 2 ** 30, 'foo'   # Should be a PyInt object.
            )
        self.assertEqual(self.last_exception.args[0],
                         "Integer property value out of range.")

    def testReallyBigArrayIndicesRaiseValueError(self):
        cx = pydermonkey.Runtime().new_context()
        obj = cx.new_object()
        self.assertRaises(
            ValueError,
            cx.define_property,
            obj, sys.maxint + 1, 'foo'   # Should be a PyLong object.
            )
        self.assertEqual(self.last_exception.args[0],
                         "Integer property value out of range.")

    def testScriptHasFilenameMember(self):
        cx = pydermonkey.Runtime().new_context()
        script = cx.compile_script('foo', '<string>', 1)
        self.assertEqual(script.filename, '<string>')

    def testScriptHasLineInfo(self):
        cx = pydermonkey.Runtime().new_context()
        script = cx.compile_script('foo\nbar', '<string>', 1)
        self.assertEqual(script.base_lineno, 1)
        self.assertEqual(script.line_extent, 2)

    def testScriptIsExposedAsBuffer(self):
        rt = pydermonkey.Runtime()
        cx = rt.new_context()
        script = cx.compile_script('foo', '<string>', 1)
        self.assertTrue(len(buffer(script)) > 0)

    def testCompileScriptWorks(self):
        self.assertEqual(self._execjs('5 + 1'), 6)

    def testErrorsRaisedIncludeStrings(self):
        self.assertRaises(pydermonkey.error, self._evaljs, 'boop()')
        self.assertEqual(self.last_exception.args[1],
                         u'ReferenceError: boop is not defined')

    def testThreadSafetyExceptionIsRaised(self):
        stuff = {}
        def make_runtime():
            stuff['rt'] = pydermonkey.Runtime()
        thread = threading.Thread(target = make_runtime)
        thread.start()
        thread.join()
        self.assertRaises(pydermonkey.error,
                          stuff['rt'].new_context)
        self.assertEqual(self.last_exception.args[0],
                         'Function called from wrong thread')
        del stuff['rt']

    def testClearObjectPrivateWorks(self):
        class Foo(object):
            pass
        pyobj = Foo()
        cx = pydermonkey.Runtime().new_context()
        obj = cx.new_object(pyobj)
        pyobj = weakref.ref(pyobj)
        self.assertEqual(pyobj(), cx.get_object_private(obj))
        cx.clear_object_private(obj)
        self.assertEqual(cx.get_object_private(obj), None)
        self.assertEqual(pyobj(), None)

    def testGetObjectPrivateWorks(self):
        class Foo(object):
            pass
        pyobj = Foo()
        cx = pydermonkey.Runtime().new_context()
        obj = cx.new_object(pyobj)
        pyobj = weakref.ref(pyobj)
        self.assertEqual(pyobj(), cx.get_object_private(obj))
        del obj
        del cx
        self.assertEqual(pyobj(), None)

    def testContextSupportsCyclicGc(self):
        def makecx():
            cx = pydermonkey.Runtime().new_context()

            def opcb(othercx):
                return cx

            cx.set_operation_callback(opcb)
            return cx

        gc.disable()
        cx = makecx()
        wcx = weakref.ref(cx)
        self.assertEqual(wcx(), cx)
        del cx
        self.assertTrue(wcx())
        gc.enable()
        gc.collect()
        self.assertEqual(wcx(), None)

    def testKeyboardInterruptStopsScript(self):
        # Let's be super-evil and have multiple interleavings of the JS
        # stack with the Python stack.

        def opcb(cx):
            raise KeyboardInterrupt()

        cx = pydermonkey.Runtime().new_context()
        cx.set_operation_callback(opcb)
        obj = cx.new_object()
        cx.init_standard_classes(obj)

        def func(cx, this, args):
            cx.evaluate_script(
                this,
                'try { while (1) {} } catch (e) {}',
                '<string>', 1
                )

        cx.define_property(obj,
                           'func',
                           cx.new_function(func, 'func'))

        def watchdog():
            time.sleep(0.1)
            cx.trigger_operation_callback()

        thread = threading.Thread(target = watchdog)
        thread.start()

        self.assertRaises(
            KeyboardInterrupt,
            cx.evaluate_script,
            obj, 'while (1) { func(); }', '<string>', 1
            )

    def testOperationCallbackIsCalled(self):
        def opcb(cx):
            raise Exception("stop eet!")

        cx = pydermonkey.Runtime().new_context()
        cx.set_operation_callback(opcb)
        obj = cx.new_object()
        cx.init_standard_classes(obj)

        def watchdog():
            time.sleep(0.1)
            cx.trigger_operation_callback()

        thread = threading.Thread(target = watchdog)
        thread.start()

        self.assertRaises(
            pydermonkey.error,
            cx.evaluate_script,
            obj, 'while (1) {}', '<string>', 1
            )

    def testUndefinedStrIsUndefined(self):
        self.assertEqual(str(pydermonkey.undefined),
                         "pydermonkey.undefined")

    def testScriptedJsFuncHasIsPythonFalse(self):
        cx = pydermonkey.Runtime().new_context()
        jsfunc = cx.evaluate_script(cx.new_object(), 
                                    '(function(){})', '<string>', 1)
        self.assertFalse(jsfunc.is_python)

    def testRecreatedJsWrappedPythonFuncHasIsPythonTrue(self):
        def foo(cx, this, args):
            pass

        cx = pydermonkey.Runtime().new_context()
        obj = cx.new_object()
        cx.define_property(obj, 'foo',
                           cx.new_function(foo, foo.__name__))
        self.assertTrue(cx.get_property(obj, 'foo').is_python)

    def testJsWrappedPythonFuncHasIsPythonTrue(self):
        def foo(cx, this, args):
            pass

        cx = pydermonkey.Runtime().new_context()
        jsfunc = cx.new_function(foo, foo.__name__)
        self.assertTrue(jsfunc.is_python)

    def testJsWrappedPythonFuncHasNoFilename(self):
        def foo(cx, this, args):
            pass

        cx = pydermonkey.Runtime().new_context()
        jsfunc = cx.new_function(foo, foo.__name__)
        self.assertEqual(jsfunc.filename, None)

    def testJsScriptedFuncHasNoPrivate(self):
        cx = pydermonkey.Runtime().new_context()
        jsfunc = cx.evaluate_script(cx.new_object(),
                                    '(function(){})', '<string>', 1)
        self.assertEqual(cx.get_object_private(jsfunc), None)

    def testGetPendingExceptionReturnsNone(self):
        cx = pydermonkey.Runtime().new_context()
        self.assertFalse(cx.is_exception_pending())
        self.assertEqual(cx.get_pending_exception(), None)

    def testThrowHookWorks(self):
        exceptions = []
        def throwhook(cx):
            self.assertTrue(cx.is_exception_pending())
            exceptions.append(cx.get_pending_exception())

        cx = pydermonkey.Runtime().new_context()
        cx.set_throw_hook(throwhook)
        self.assertRaises(
            pydermonkey.error,
            cx.evaluate_script,
            cx.new_object(),
            '(function() { throw "hi"; })()',
            '<string>', 1
            )
        self.assertEqual(exceptions, ['hi', 'hi'])
        self.assertFalse(cx.is_exception_pending())
        self.assertEqual(cx.get_pending_exception(), None)

    def testJsWrappedPythonFuncHasPrivate(self):
        def foo(cx, this, args):
            pass

        cx = pydermonkey.Runtime().new_context()
        jsfunc = cx.new_function(foo, foo.__name__)
        self.assertEqual(cx.get_object_private(jsfunc), foo)

    def testJsWrappedPythonFuncIsNotGCd(self):
        def define(cx, obj):
            def func(cx, this, args):
                return u'func was called'
            jsfunc = cx.new_function(func, func.__name__)
            cx.define_property(obj, func.__name__, jsfunc)
            return weakref.ref(func)
        rt = pydermonkey.Runtime()
        cx = rt.new_context()
        obj = cx.new_object()
        cx.init_standard_classes(obj)
        ref = define(cx, obj)
        cx.gc()
        self.assertNotEqual(ref(), None)
        result = cx.evaluate_script(obj, 'func()', '<string>', 1)
        self.assertEqual(result, u'func was called')

        # Now ensure that the wrapped function is GC'd when it's
        # no longer reachable from JS space.
        cx.define_property(obj, 'func', 0)
        cx.gc()
        self.assertEqual(ref(), None)

    def testCircularJsWrappedPythonFuncIsGCdIfPrivateCleared(self):
        def define(cx, obj):
            rt = cx.get_runtime()
            def func(cx, this, args):
                # Oh noes, a circular reference is born!
                rt
            jsfunc = cx.new_function(func, func.__name__)
            cx.define_property(obj, func.__name__, jsfunc)
            return (jsfunc, weakref.ref(func))
        rt = pydermonkey.Runtime()
        cx = rt.new_context()
        obj = cx.new_object()
        cx.init_standard_classes(obj)
        jsfunc, ref = define(cx, obj)

        # This will break the circular reference.
        cx.clear_object_private(jsfunc)

        del jsfunc
        del rt
        del cx
        del obj
        self.assertEqual(ref(), None)

    def testFunctionsWithClosuresAreNotIdentical(self):
        cx = pydermonkey.Runtime().new_context()
        obj = cx.new_object()
        cx.init_standard_classes(obj)
        cx.evaluate_script(
            obj, "function build(x) { return function foo() { return x; } }",
            "<string>", 1
            )
        func1 = cx.evaluate_script(obj, "build(1)", "<string>", 1)
        func2 = cx.evaluate_script(obj, "build(2)", "<string>", 1)
        self.assertNotEqual(func1, func2)
        self.assertEqual(func1.name, 'foo')
        self.assertEqual(func1.name, func2.name)

    def testAnonymousJsFunctionHasNullNameAttribute(self):
        cx = pydermonkey.Runtime().new_context()
        obj = cx.new_object()
        cx.init_standard_classes(obj)
        jsfunc = cx.evaluate_script(obj, "(function() {})",
                                    "<string>", 1)
        self.assertEqual(jsfunc.name, None)

    def testJsFunctionHasNameAttribute(self):
        cx = pydermonkey.Runtime().new_context()
        obj = cx.new_object()
        cx.init_standard_classes(obj)
        jsfunc = cx.evaluate_script(obj, "(function blarg() {})",
                                    "<string>", 1)
        self.assertEqual(jsfunc.name, "blarg")

    def testJsWrappedPythonFuncHasNameAttribute(self):
        def func(cx, this, args):
            return True

        cx = pydermonkey.Runtime().new_context()
        jsfunc = cx.new_function(func, "foo")
        self.assertEqual(jsfunc.name, "foo")

    def testJsWrappedPythonFuncIsGCdAtRuntimeDestruction(self):
        def define(cx, obj):
            def func(cx, this, args):
                return u'func was called'
            jsfunc = cx.new_function(func, func.__name__)
            cx.define_property(obj, func.__name__, jsfunc)
            return weakref.ref(func)
        rt = pydermonkey.Runtime()
        cx = rt.new_context()
        obj = cx.new_object()
        cx.init_standard_classes(obj)
        ref = define(cx, obj)
        del rt
        del cx
        del obj
        self.assertEqual(ref(), None)

    def testJsWrappedPythonFuncThrowsExcIfPrivateCleared(self):
        def func(cx, this, args):
            return True

        code = "func()"
        cx = pydermonkey.Runtime().new_context()
        obj = cx.new_object()
        cx.init_standard_classes(obj)
        jsfunc = cx.new_function(func, func.__name__)
        cx.define_property(obj, func.__name__, jsfunc)
        cx.clear_object_private(jsfunc)
        self.assertRaises(pydermonkey.error,
                          cx.evaluate_script,
                          obj, code, '<string>', 1)
        self.assertEqual(
            self._tostring(cx, self.last_exception.args[0]),
            "Error: Wrapped Python function no longer exists"
            )

    def testJsWrappedPythonFuncPassesContext(self):
        contexts = []

        def func(cx, this, args):
            contexts.append(cx)
            return True

        code = "func()"
        cx = pydermonkey.Runtime().new_context()
        obj = cx.new_object()
        cx.init_standard_classes(obj)
        jsfunc = cx.new_function(func, func.__name__)
        self._clearOnTeardown(jsfunc)
        cx.define_property(obj, func.__name__, jsfunc)
        cx.evaluate_script(obj, code, '<string>', 1)
        self.assertEqual(contexts[0], cx)

    def testJsWrappedPythonFuncPassesThisArg(self):
        thisObjs = []

        def func(cx, this, args):
            thisObjs.append(this)
            return True

        code = "func()"
        cx = pydermonkey.Runtime().new_context()
        obj = cx.new_object()
        cx.init_standard_classes(obj)
        jsfunc = cx.new_function(func, func.__name__)
        self._clearOnTeardown(jsfunc)
        cx.define_property(obj, func.__name__, jsfunc)
        cx.evaluate_script(obj, code, '<string>', 1)
        self.assertEqual(thisObjs[0], obj)

    def testJsWrappedPythonFuncPassesFuncArgs(self):
        funcArgs = []

        def func(cx, this, args):
            funcArgs.append(args)
            return True

        cx = pydermonkey.Runtime().new_context()
        obj = cx.new_object()
        cx.init_standard_classes(obj)
        jsfunc = cx.new_function(func, func.__name__)
        self._clearOnTeardown(jsfunc)

        cx.define_property(obj, func.__name__, jsfunc)

        cx.evaluate_script(obj, "func()", '<string>', 1)
        self.assertEqual(len(funcArgs[0]), 0)
        self.assertTrue(isinstance(funcArgs[0], tuple))

        cx.evaluate_script(obj, "func(1, 'foo')", '<string>', 1)
        self.assertEqual(len(funcArgs[1]), 2)
        self.assertEqual(funcArgs[1][0], 1)
        self.assertEqual(funcArgs[1][1], u'foo')

    def testJsWrappedPythonFunctionReturnsUnicodeWithEmbeddedNULs(self):
        def hai2u(cx, this, args):
            return args[0] + u"o hai"
        self.assertEqual(self._evalJsWrappedPyFunc(hai2u,
                                                   'hai2u("blah\x00 ")'),
                         u"blah\x00 o hai")

    def testJsWrappedPythonFunctionReturnsUndefined(self):
        def hai2u(cx, this, args):
            return pydermonkey.undefined
        self.assertEqual(self._evalJsWrappedPyFunc(hai2u, 'hai2u()'),
                         pydermonkey.undefined)

    def testJsWrappedPythonFunctionReturnsString(self):
        def hai2u(cx, this, args):
            return "o hai"
        self.assertEqual(self._evalJsWrappedPyFunc(hai2u, 'hai2u()'),
                         "o hai")

    def testJsWrappedPythonFunctionReturnsUnicode(self):
        def hai2u(cx, this, args):
            return u"o hai\u2026"
        self.assertEqual(self._evalJsWrappedPyFunc(hai2u, 'hai2u()'),
                         u"o hai\u2026")

    def testJsWrappedPythonFunctionThrowsJsException(self):
        def hai2u(cx, this, args):
            raise pydermonkey.error(u"blarg")
        self.assertRaises(pydermonkey.error,
                          self._evalJsWrappedPyFunc,
                          hai2u, 'hai2u()')
        self.assertEqual(self.last_exception.args[0], u"blarg")

    def testJsWrappedPythonFunctionThrowsJsException2(self):
        def hai2u(cx, this, args):
            cx.evaluate_script(this, 'throw "blarg"', '<string>', 1)
        self.assertRaises(pydermonkey.error,
                          self._evalJsWrappedPyFunc,
                          hai2u, 'hai2u()')
        self.assertEqual(self.last_exception.args[0], u"blarg")

    def testJsWrappedPythonFunctionThrowsPyException(self):
        thecx = []
        def hai2u(cx, this, args):
            thecx.append(cx)
            raise Exception("hello")
        self.assertRaises(pydermonkey.error,
                          self._evalJsWrappedPyFunc,
                          hai2u, 'hai2u()')
        exc = thecx[0].get_object_private(self.last_exception.args[0])
        self.assertEqual(exc.args[0], "hello")

    def testJsWrappedPythonFunctionReturnsNone(self):
        def hai2u(cx, this, args):
            pass
        self.assertEqual(self._evalJsWrappedPyFunc(hai2u, 'hai2u()'),
                         None)

    def testJsWrappedPythonFunctionReturnsTrue(self):
        def hai2u(cx, this, args):
            return True
        self.assertEqual(self._evalJsWrappedPyFunc(hai2u, 'hai2u()'),
                         True)

    def testJsWrappedPythonFunctionReturnsFalse(self):
        def hai2u(cx, this, args):
            return False
        self.assertEqual(self._evalJsWrappedPyFunc(hai2u, 'hai2u()'),
                         False)

    def testJsWrappedPythonFunctionReturnsSmallInt(self):
        def hai2u(cx, this, args):
            return 5
        self.assertEqual(self._evalJsWrappedPyFunc(hai2u, 'hai2u()'),
                         5)

    def testJsWrappedPythonFunctionReturnsFloat(self):
        def hai2u(cx, this, args):
            return 5.1
        self.assertEqual(self._evalJsWrappedPyFunc(hai2u, 'hai2u()'),
                         5.1)

    def testJsWrappedPythonFunctionReturnsNegativeInt(self):
        def hai2u(cx, this, args):
            return -5
        self.assertEqual(self._evalJsWrappedPyFunc(hai2u, 'hai2u()'),
                         -5)

    def testJsWrappedPythonFunctionReturnsBigInt(self):
        def hai2u(cx, this, args):
            return 2147483647
        self.assertEqual(self._evalJsWrappedPyFunc(hai2u, 'hai2u()'),
                         2147483647)

    def testDefinePropertyWorksWithIntegers(self):
        cx = pydermonkey.Runtime().new_context()
        obj = cx.new_object()
        cx.define_property(obj, 0, 'test')
        self.assertEqual(cx.evaluate_script(obj, "this[0]", '<string>', 1),
                         'test')

    def testGetPropertyDoesNotWorkWithFloats(self):
        cx = pydermonkey.Runtime().new_context()
        obj = cx.new_object()
        self.assertRaises(TypeError,
                          cx.get_property,
                          obj, 0.534)
        self.assertEqual(self.last_exception.args[0],
                         'Property must be a string or integer.')

    def testHasPropertyWorksWithIntegers(self):
        cx = pydermonkey.Runtime().new_context()
        obj = cx.new_object()
        self.assertEqual(cx.has_property(obj, 0), False)
        cx.define_property(obj, 0, 'hi')
        self.assertEqual(cx.has_property(obj, 0), True)

    def testGetPropertyWorksWithIntegers(self):
        cx = pydermonkey.Runtime().new_context()
        obj = cx.new_object()
        cx.init_standard_classes(obj)
        array = cx.evaluate_script(obj, "['test']", '<string>', 1)
        self.assertEqual(cx.get_property(array, 0), 'test')

    def testHasPropertyWorks(self):
        cx = pydermonkey.Runtime().new_context()
        obj = cx.new_object()
        cx.init_standard_classes(obj)
        foo = cx.new_object()
        cx.define_property(obj, u"foo\u2026", foo)
        self.assertTrue(cx.has_property(obj, u"foo\u2026"))
        self.assertFalse(cx.has_property(obj, "bar"))

    def testDefinePropertyWorksWithUnicodePropertyNames(self):
        cx = pydermonkey.Runtime().new_context()
        obj = cx.new_object()
        cx.init_standard_classes(obj)
        foo = cx.new_object()
        cx.define_property(obj, u"foo\u2026", foo)
        self.assertEqual(
            cx.get_property(obj, u"foo\u2026"),
            foo
            )

    def testDefinePropertyWorksWithObject(self):
        cx = pydermonkey.Runtime().new_context()
        obj = cx.new_object()
        cx.init_standard_classes(obj)
        foo = cx.new_object()
        cx.define_property(obj, "foo", foo)
        self.assertEqual(
            cx.evaluate_script(obj, 'foo', '<string>', 1),
            foo
            )

    def testDefinePropertyWorksWithString(self):
        cx = pydermonkey.Runtime().new_context()
        obj = cx.new_object()
        cx.init_standard_classes(obj)
        foo = cx.new_object()
        cx.define_property(obj, "foo", u"hello")
        self.assertEqual(
            cx.evaluate_script(obj, 'foo', '<string>', 1),
            u"hello"
            )

    def testObjectIsIdentityPreserving(self):
        cx = pydermonkey.Runtime().new_context()
        obj = cx.new_object()
        cx.init_standard_classes(obj)
        cx.evaluate_script(obj, 'var foo = {bar: 1}', '<string>', 1)
        self.assertTrue(isinstance(cx.get_property(obj, u"foo"),
                                   pydermonkey.Object))
        self.assertTrue(cx.get_property(obj, u"foo") is
                        cx.get_property(obj, "foo"))

    def testObjectGetattrThrowsException(self):
        cx = pydermonkey.Runtime().new_context()
        obj = cx.new_object()
        cx.init_standard_classes(obj)
        result = cx.evaluate_script(obj, '({get foo() { throw "blah"; }})',
                                    '<string>', 1)
        self.assertRaises(pydermonkey.error,
                          cx.get_property,
                          result,
                          u"foo")
        self.assertEqual(self.last_exception.args[0], u"blah")

    def testInfiniteRecursionRaisesError(self):
        cx = pydermonkey.Runtime().new_context()
        obj = cx.new_object()
        cx.init_standard_classes(obj)
        self.assertRaises(
            pydermonkey.error,
            cx.evaluate_script,
            obj, '(function foo() { foo(); })();', '<string>', 1
            )
        self.assertEqual(
            self._tostring(cx, self.last_exception.args[0]),
            "InternalError: too much recursion"
            )

    def testObjectGetattrWorks(self):
        cx = pydermonkey.Runtime().new_context()
        obj = cx.new_object()
        cx.init_standard_classes(obj)
        cx.evaluate_script(obj, 'var boop = 5', '<string>', 1)
        cx.evaluate_script(obj, 'this["blarg\u2026"] = 5', '<string>', 1)
        self.assertEqual(cx.get_property(obj, u"beans"),
                         pydermonkey.undefined)
        self.assertEqual(cx.get_property(obj, u"blarg\u2026"), 5)
        self.assertEqual(cx.get_property(obj, u"boop"), 5)

    def testContextIsInstance(self):
        cx = pydermonkey.Runtime().new_context()
        self.assertTrue(isinstance(cx, pydermonkey.Context))

    def testContextTypeCannotBeInstantiated(self):
        self.assertRaises(TypeError, pydermonkey.Context)

    def testObjectIsInstance(self):
        obj = pydermonkey.Runtime().new_context().new_object()
        self.assertTrue(isinstance(obj, pydermonkey.Object))
        self.assertFalse(isinstance(obj, pydermonkey.Function))

    def testObjectTypeCannotBeInstantiated(self):
        self.assertRaises(TypeError, pydermonkey.Object)

    def testFunctionIsInstance(self):
        def boop():
            pass
        obj = pydermonkey.Runtime().new_context().new_function(boop, "boop")
        self.assertTrue(isinstance(obj, pydermonkey.Object))
        self.assertTrue(isinstance(obj, pydermonkey.Function))

    def testFunctionTypeCannotBeInstantiated(self):
        self.assertRaises(TypeError, pydermonkey.Function)

    def testObjectGetRuntimeWorks(self):
        rt = pydermonkey.Runtime()
        obj = rt.new_context().new_object()
        self.assertEqual(obj.get_runtime(), rt)

    def testContextGetRuntimeWorks(self):
        rt = pydermonkey.Runtime()
        cx = rt.new_context()
        self.assertEqual(cx.get_runtime(), rt)

    def testRuntimesAreWeakReferencable(self):
        rt = pydermonkey.Runtime()
        wrt = weakref.ref(rt)
        self.assertEqual(rt, wrt())
        del rt
        self.assertEqual(wrt(), None)

    def testContextsAreWeakReferencable(self):
        rt = pydermonkey.Runtime()
        cx = rt.new_context()
        wcx = weakref.ref(cx)
        self.assertEqual(cx, wcx())
        del cx
        self.assertEqual(wcx(), None)

    def testUndefinedCannotBeInstantiated(self):
        self.assertRaises(TypeError, pydermonkey.undefined)

    def testEvaluateThrowsException(self):
        cx = pydermonkey.Runtime().new_context()
        obj = cx.new_object()
        self.assertRaises(pydermonkey.error,
                          cx.evaluate_script,
                          obj, 'hai2u()', '<string>', 1)
        self.assertEqual(self._tostring(cx,
                                        self.last_exception.args[0]),
                         'ReferenceError: hai2u is not defined')

    def testThrowingObjWithBadToStringWorks(self):
        self.assertRaises(
            pydermonkey.error,
            self._evaljs,
            "throw {toString: function() { throw 'dujg' }}"
            )
        self.assertEqual(
            self.last_exception.args[1],
            "<string conversion failed>"
            )

    def testEvaluateTakesUnicodeCode(self):
        self.assertEqual(self._evaljs(u"'foo\u2026'"),
                         u"foo\u2026")

    def testEvaluateReturnsUndefined(self):
        retval = self._evaljs("")
        self.assertTrue(retval is pydermonkey.undefined)

    def testEvaludateReturnsUnicodeWithEmbeddedNULs(self):
        retval = self._evaljs("'\x00hi'")
        self.assertEqual(retval, u'\x00hi')

    def testEvaluateReturnsSMPUnicode(self):
        # This is 'LINEAR B SYLLABLE B008 A', in the supplementary
        # multilingual plane (SMP).
        retval = self._evaljs("'\uD800\uDC00'")
        self.assertEqual(retval, u'\U00010000')
        self.assertEqual(retval.encode('utf-16'),
                         '\xff\xfe\x00\xd8\x00\xdc')

    def testEvaluateReturnsBMPUnicode(self):
        retval = self._evaljs("'o hai\u2026'")
        self.assertTrue(type(retval) == unicode)
        self.assertEqual(retval, u'o hai\u2026')

    def testEvaluateReturnsObject(self):
        cx = pydermonkey.Runtime().new_context()
        obj = cx.new_object()
        cx.init_standard_classes(obj)
        obj = cx.evaluate_script(obj, '({boop: 1})', '<string>', 1)
        self.assertTrue(isinstance(obj, pydermonkey.Object))
        self.assertEqual(cx.get_property(obj, u"boop"), 1)

    def testScriptedFunctionsHaveFilenameInfo(self):
        cx = pydermonkey.Runtime().new_context()
        obj = cx.new_object()
        cx.init_standard_classes(obj)
        jsfunc = cx.evaluate_script(obj,
                                    '(function boop() { \nreturn 1; })',
                                    'somefile', 5)
        self.assertEqual(jsfunc.filename, 'somefile')
        self.assertEqual(jsfunc.base_lineno, 5)
        self.assertEqual(jsfunc.line_extent, 2)

    def testEvaluateReturnsFunction(self):
        cx = pydermonkey.Runtime().new_context()
        obj = cx.new_object()
        cx.init_standard_classes(obj)
        obj = cx.evaluate_script(obj, '(function boop() { return 1; })',
                                 '<string>', 1)
        self.assertTrue(isinstance(obj, pydermonkey.Function))

    def testJsExceptionStateIsClearedAfterExceptionIsCaught(self):
        cx = pydermonkey.Runtime().new_context()
        obj = cx.new_object()
        self.assertRaises(pydermonkey.error,
                          cx.evaluate_script,
                          obj, 'blah()', '<string>', 1)
        self.assertEqual(cx.evaluate_script(obj, '5+3', '<string>', 1),
                         8)

    def testTypeConversionNotImplementedMentionsTypeName(self):
        cx = pydermonkey.Runtime().new_context()
        obj = cx.new_object()
        self.assertRaises(
            NotImplementedError,
            cx.define_property,
            obj, 'hi', 2 ** 91
            )
        self.assertEqual(
            self.last_exception.args[0],
            "Data type conversion not implemented for type 'long'."
            )

    def testCallFunctionRaisesErrorOnBadFuncArgs(self):
        cx = pydermonkey.Runtime().new_context()
        obj = cx.new_object()
        obj = cx.evaluate_script(
            obj,
            '(function boop(a, b) { return a+b+this.c; })',
            '<string>', 1
            )
        self.assertRaises(
            NotImplementedError,
            cx.call_function,
            obj, obj, (1, self)
            )

    def _tostring(self, cx, obj):
        return cx.call_function(obj,
                                cx.get_property(obj, u"toString"),
                                ())

    def testCallFunctionRaisesErrorFromJS(self):
        cx = pydermonkey.Runtime().new_context()
        obj = cx.new_object()
        obj = cx.evaluate_script(
            obj,
            '(function boop(a, b) { blarg(); })',
            '<string>', 1
            )
        self.assertRaises(pydermonkey.error,
                          cx.call_function,
                          obj, obj, (1,))
        self.assertEqual(self._tostring(cx,
                                        self.last_exception.args[0]),
                         'ReferenceError: blarg is not defined')

    def testInitStandardClassesRaisesExcOnRuntimeMismatch(self):
        cx2 = pydermonkey.Runtime().new_context()
        cx = pydermonkey.Runtime().new_context()
        obj = cx.new_object()
        self.assertRaises(ValueError,
                          cx2.init_standard_classes,
                          obj)
        self.assertEqual(self.last_exception.args[0],
                         'JS runtime mismatch')

    def testCallFunctionWorks(self):
        cx = pydermonkey.Runtime().new_context()
        obj = cx.new_object()
        thisArg = cx.new_object()
        cx.define_property(thisArg, "c", 3)
        cx.init_standard_classes(obj)
        obj = cx.evaluate_script(
            obj,
            '(function boop(a, b) { return a+b+this.c; })',
            '<string>', 1
            )
        self.assertEqual(cx.call_function(thisArg, obj, (1,2)), 6)

    def testEvaluateReturnsTrue(self):
        self.assertTrue(self._evaljs('true') is True)

    def testEvaluateReturnsFalse(self):
        self.assertTrue(self._evaljs('false') is False)

    def testEvaluateReturnsNone(self):
        self.assertTrue(self._evaljs('null') is None)

    def testEvaluateReturnsIntegers(self):
        self.assertEqual(self._evaljs('1+3'), 4)

    def testEvaluateReturnsNegativeIntegers(self):
        self.assertEqual(self._evaljs('-5'), -5)

    def testEvaluateReturnsBigIntegers(self):
        self.assertEqual(self._evaljs('2147483647*2'),
                         2147483647*2)

    def testEvaluateReturnsFloats(self):
        self.assertEqual(self._evaljs('1.1+3'), 4.1)

if __name__ == '__main__':
    unittest.main()
