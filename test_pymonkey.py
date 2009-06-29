import unittest
import pymonkey

class PymonkeyTests(unittest.TestCase):
    def testJSRuntimeWorks(self):
        rt = pymonkey.Runtime()
        cx = rt.new_context()
        self.assertRaises(TypeError, pymonkey.Context)
        self.assertTrue(isinstance(cx, pymonkey.Context))
        self.assertEqual(cx.get_runtime(), rt)

        obj = cx.new_object()
        self.assertRaises(TypeError, pymonkey.Object)
        self.assertTrue(isinstance(obj, pymonkey.Object))

        cx.init_standard_classes(obj)

    def testUndefinedCannotBeInstantiated(self):
        self.assertRaises(TypeError, pymonkey.undefined)

    def testEvaluateReturnsUndefined(self):
        retval = pymonkey.evaluate("", '<string>', 1)
        self.assertTrue(retval is pymonkey.undefined)

    def testEvaluateReturnsUnicode(self):
        retval = pymonkey.evaluate("'o hai\u2026'", '<string>', 1)
        self.assertTrue(type(retval) == unicode)
        self.assertEqual(retval, u'o hai\u2026')

    def testEvaluateReturnsTrue(self):
        self.assertTrue(pymonkey.evaluate('true', '<string>', 1) is True)

    def testEvaluateReturnsFalse(self):
        self.assertTrue(pymonkey.evaluate('false', '<string>', 1) is False)

    def testEvaluateReturnsNone(self):
        self.assertTrue(pymonkey.evaluate('null', '<string>', 1) is None)

    def testEvaluateReturnsIntegers(self):
        self.assertEqual(pymonkey.evaluate('1+3', '<string>', 1),
                         4)

    def testEvaluateReturnsFloats(self):
        self.assertEqual(pymonkey.evaluate('1.1+3', '<string>', 1),
                         4.1)

if __name__ == '__main__':
    unittest.main()
