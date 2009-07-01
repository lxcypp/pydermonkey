import unittest
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
        cx.define_function(obj, func, func.__name__)
        return cx.evaluate_script(obj, code, '<string>', 1)

    def testJsWrappedPythonFunctionReturnsUnicode(self):
        def hai2u():
            return u"o hai"
        self.assertEqual(self._evalJsWrappedPyFunc(hai2u, 'hai2u()'),
                         u"o hai")

    def testJsWrappedPythonFunctionReturnsSmallInt(self):
        def hai2u():
            return 5
        self.assertEqual(self._evalJsWrappedPyFunc(hai2u, 'hai2u()'),
                         5)

    def testJsWrappedPythonFunctionReturnsFloat(self):
        def hai2u():
            return 5.1
        self.assertEqual(self._evalJsWrappedPyFunc(hai2u, 'hai2u()'),
                         5.1)

    def testJsWrappedPythonFunctionReturnsNegativeInt(self):
        def hai2u():
            return -5
        self.assertEqual(self._evalJsWrappedPyFunc(hai2u, 'hai2u()'),
                         -5)

    def testJsWrappedPythonFunctionReturnsBigInt(self):
        def hai2u():
            return 2147483647
        self.assertEqual(self._evalJsWrappedPyFunc(hai2u, 'hai2u()'),
                         2147483647)

    def testObjectIsIdentityPreserving(self):
        cx = pymonkey.Runtime().new_context()
        obj = cx.new_object()
        cx.init_standard_classes(obj)
        cx.evaluate_script(obj, 'foo = {bar: 1}', '<string>', 1)
        self.assertTrue(isinstance(cx.get_property(obj, u"foo"),
                                   pymonkey.Object))
        self.assertTrue(cx.get_property(obj, u"foo") is
                        cx.get_property(obj, u"foo"))

    def testObjectGetattrWorks(self):
        cx = pymonkey.Runtime().new_context()
        obj = cx.new_object()
        cx.init_standard_classes(obj)
        cx.evaluate_script(obj, 'boop = 5', '<string>', 1)
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

    def testObjectTypeCannotBeInstantiated(self):
        self.assertRaises(TypeError, pymonkey.Object)

    def testGetRuntimeWorks(self):
        rt = pymonkey.Runtime()
        cx = rt.new_context()
        self.assertEqual(cx.get_runtime(), rt)

    def testUndefinedCannotBeInstantiated(self):
        self.assertRaises(TypeError, pymonkey.undefined)

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
        rt = pymonkey.Runtime()
        cx = rt.new_context()
        obj = cx.new_object()
        cx.init_standard_classes(obj)
        obj = cx.evaluate_script(obj, '({boop: 1})', '<string>', 1)
        self.assertTrue(isinstance(obj, pymonkey.Object))
        self.assertEqual(cx.get_property(obj, u"boop"), 1)

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
