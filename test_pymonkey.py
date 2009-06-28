import unittest
import pymonkey

class PymonkeyTests(unittest.TestCase):
    def testEvaluateReturnsIntegers(self):
        self.assertEqual(pymonkey.evaluate('1+3', '<string>', 1),
                         4)

    def testEvaluateReturnsFloats(self):
        self.assertEqual(pymonkey.evaluate('1.1+3', '<string>', 1),
                         4.1)

if __name__ == '__main__':
    unittest.main()
