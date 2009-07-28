import sys
import unittest
import weakref
import time
import threading

import pymonkey

class PymonkeyTests(unittest.TestCase):
    def _evaljs(self, code):
        rt = pymonkey.Runtime()
        cx = rt.new_context()
        obj = cx.new_object()
        cx.init_standard_classes(obj)
        return cx.evaluate_script(obj, code, '<string>', 1)

    def _evalJsWrappedPyFunc(self, func, code):
        cx = pymonkey.Runtime().new_context()
        obj = cx.new_object()
        cx.init_standard_classes(obj)
        jsfunc = cx.new_function(func, func.__name__)
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

    def testGetObjectPrivateWorks(self):
        class Foo(object):
            pass
        pyobj = Foo()
        cx = pymonkey.Runtime().new_context()
        obj = cx.new_object(pyobj)
        pyobj = weakref.ref(pyobj)
        self.assertEqual(pyobj(), cx.get_object_private(obj))
        del obj
        del cx
        self.assertEqual(pyobj(), None)

    def testOperationCallbackIsCalled(self):
        def opcb(cx):
            raise Exception("stop eet!")

        cx = pymonkey.Runtime().new_context()
        cx.set_operation_callback(opcb)
        obj = cx.new_object()
        cx.init_standard_classes(obj)

        def watchdog():
            time.sleep(0.1)
            cx.trigger_operation_callback()

        thread = threading.Thread(target = watchdog)
        thread.start()

        self.assertRaises(
            pymonkey.error,
            cx.evaluate_script,
            obj, 'while (1) {}', '<string>', 1
            )
        self.assertEqual(self.last_exception.message,
                         "stop eet!")

    def testUndefinedStrIsUndefined(self):
        self.assertEqual(str(pymonkey.undefined),
                         "pymonkey.undefined")

    def testJsWrappedPythonFuncIsNotGCd(self):
        def define(cx, obj):
            def func(cx, this, args):
                return u'func was called'
            jsfunc = cx.new_function(func, func.__name__)
            cx.define_property(obj, func.__name__, jsfunc)
            return weakref.ref(func)
        rt = pymonkey.Runtime()
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

    def testJsWrappedPythonFuncIsGCdAtRuntimeDestruction(self):
        def define(cx, obj):
            def func(cx, this, args):
                return u'func was called'
            jsfunc = cx.new_function(func, func.__name__)
            cx.define_property(obj, func.__name__, jsfunc)
            return weakref.ref(func)
        rt = pymonkey.Runtime()
        cx = rt.new_context()
        obj = cx.new_object()
        cx.init_standard_classes(obj)
        ref = define(cx, obj)
        del rt
        del cx
        del obj
        self.assertEqual(ref(), None)

    def testJsWrappedPythonFuncPassesContext(self):
        contexts = []

        def func(cx, this, args):
            contexts.append(cx)
            return True

        code = "func()"
        cx = pymonkey.Runtime().new_context()
        obj = cx.new_object()
        cx.init_standard_classes(obj)
        jsfunc = cx.new_function(func, func.__name__)
        cx.define_property(obj, func.__name__, jsfunc)
        cx.evaluate_script(obj, code, '<string>', 1)
        self.assertEqual(contexts[0], cx)

    def testJsWrappedPythonFuncPassesThisArg(self):
        thisObjs = []

        def func(cx, this, args):
            thisObjs.append(this)
            return True

        code = "func()"
        cx = pymonkey.Runtime().new_context()
        obj = cx.new_object()
        cx.init_standard_classes(obj)
        jsfunc = cx.new_function(func, func.__name__)
        cx.define_property(obj, func.__name__, jsfunc)
        cx.evaluate_script(obj, code, '<string>', 1)
        self.assertEqual(thisObjs[0], obj)

    def testJsWrappedPythonFuncPassesFuncArgs(self):
        funcArgs = []

        def func(cx, this, args):
            funcArgs.append(args)
            return True

        cx = pymonkey.Runtime().new_context()
        obj = cx.new_object()
        cx.init_standard_classes(obj)
        jsfunc = cx.new_function(func, func.__name__)
        cx.define_property(obj, func.__name__, jsfunc)

        cx.evaluate_script(obj, "func()", '<string>', 1)
        self.assertEqual(len(funcArgs[0]), 0)
        self.assertTrue(isinstance(funcArgs[0], tuple))

        cx.evaluate_script(obj, "func(1, 'foo')", '<string>', 1)
        self.assertEqual(len(funcArgs[1]), 2)
        self.assertEqual(funcArgs[1][0], 1)
        self.assertEqual(funcArgs[1][1], u'foo')

    def testJsWrappedPythonFunctionReturnsUnicode(self):
        def hai2u(cx, this, args):
            return u"o hai"
        self.assertEqual(self._evalJsWrappedPyFunc(hai2u, 'hai2u()'),
                         u"o hai")

    def testJsWrappedPythonFunctionThrowsException(self):
        def hai2u(cx, this, args):
            raise Exception("hello")
        self.assertRaises(pymonkey.error,
                          self._evalJsWrappedPyFunc,
                          hai2u, 'hai2u()')
        self.assertEqual(self.last_exception.message,
                         "hello")

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

    def testDefinePropertyWorksWithObject(self):
        cx = pymonkey.Runtime().new_context()
        obj = cx.new_object()
        cx.init_standard_classes(obj)
        foo = cx.new_object()
        cx.define_property(obj, "foo", foo)
        self.assertEqual(
            cx.evaluate_script(obj, 'foo', '<string>', 1),
            foo
            )

    def testDefinePropertyWorksWithString(self):
        cx = pymonkey.Runtime().new_context()
        obj = cx.new_object()
        cx.init_standard_classes(obj)
        foo = cx.new_object()
        cx.define_property(obj, "foo", u"hello")
        self.assertEqual(
            cx.evaluate_script(obj, 'foo', '<string>', 1),
            u"hello"
            )

    def testObjectIsIdentityPreserving(self):
        cx = pymonkey.Runtime().new_context()
        obj = cx.new_object()
        cx.init_standard_classes(obj)
        cx.evaluate_script(obj, 'var foo = {bar: 1}', '<string>', 1)
        self.assertTrue(isinstance(cx.get_property(obj, u"foo"),
                                   pymonkey.Object))
        self.assertTrue(cx.get_property(obj, u"foo") is
                        cx.get_property(obj, u"foo"))

    def testObjectGetattrWorks(self):
        cx = pymonkey.Runtime().new_context()
        obj = cx.new_object()
        cx.init_standard_classes(obj)
        cx.evaluate_script(obj, 'var boop = 5', '<string>', 1)
        cx.evaluate_script(obj, 'this["blarg\u2026"] = 5', '<string>', 1)
        self.assertEqual(cx.get_property(obj, u"beans"),
                         pymonkey.undefined)
        self.assertEqual(cx.get_property(obj, u"blarg\u2026"), 5)
        self.assertEqual(cx.get_property(obj, u"boop"), 5)

    def testContextIsInstance(self):
        cx = pymonkey.Runtime().new_context()
        self.assertTrue(isinstance(cx, pymonkey.Context))

    def testContextTypeCannotBeInstantiated(self):
        self.assertRaises(TypeError, pymonkey.Context)

    def testObjectIsInstance(self):
        obj = pymonkey.Runtime().new_context().new_object()
        self.assertTrue(isinstance(obj, pymonkey.Object))
        self.assertFalse(isinstance(obj, pymonkey.Function))

    def testObjectTypeCannotBeInstantiated(self):
        self.assertRaises(TypeError, pymonkey.Object)

    def testFunctionIsInstance(self):
        def boop():
            pass
        obj = pymonkey.Runtime().new_context().new_function(boop, "boop")
        self.assertTrue(isinstance(obj, pymonkey.Object))
        self.assertTrue(isinstance(obj, pymonkey.Function))

    def testFunctionTypeCannotBeInstantiated(self):
        self.assertRaises(TypeError, pymonkey.Function)

    def testGetRuntimeWorks(self):
        rt = pymonkey.Runtime()
        cx = rt.new_context()
        self.assertEqual(cx.get_runtime(), rt)

    def testUndefinedCannotBeInstantiated(self):
        self.assertRaises(TypeError, pymonkey.undefined)

    def testEvaluateThrowsException(self):
        self.assertRaises(pymonkey.error,
                          self._evaljs, 'hai2u()')
        self.assertEqual(self.last_exception.message,
                         'ReferenceError: hai2u is not defined')

    def testEvaluateReturnsUndefined(self):
        retval = self._evaljs("")
        self.assertTrue(retval is pymonkey.undefined)

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
        cx = pymonkey.Runtime().new_context()
        obj = cx.new_object()
        cx.init_standard_classes(obj)
        obj = cx.evaluate_script(obj, '({boop: 1})', '<string>', 1)
        self.assertTrue(isinstance(obj, pymonkey.Object))
        self.assertEqual(cx.get_property(obj, u"boop"), 1)

    def testEvaluateReturnsFunction(self):
        cx = pymonkey.Runtime().new_context()
        obj = cx.new_object()
        cx.init_standard_classes(obj)
        obj = cx.evaluate_script(obj, '(function boop() { return 1; })',
                                 '<string>', 1)
        self.assertTrue(isinstance(obj, pymonkey.Function))

    def testCallFunctionRaisesErrorOnBadFuncArgs(self):
        cx = pymonkey.Runtime().new_context()
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

    def testCallFunctionRaisesErrorFromJS(self):
        cx = pymonkey.Runtime().new_context()
        obj = cx.new_object()
        obj = cx.evaluate_script(
            obj,
            '(function boop(a, b) { blarg(); })',
            '<string>', 1
            )
        self.assertRaises(pymonkey.error,
                          cx.call_function,
                          obj, obj, (1,))
        self.assertEqual(self.last_exception.message,
                         'ReferenceError: blarg is not defined')

    def testCallFunctionWorks(self):
        cx = pymonkey.Runtime().new_context()
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
