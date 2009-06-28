import unittest
import pymonkey

class PymonkeyTests(unittest.TestCase):
    def test_evaluate(self):
        self.assertEqual(pymonkey.evaluate('1+3', '<string>', 1),
                         4)

if __name__ == '__main__':
    unittest.main()
