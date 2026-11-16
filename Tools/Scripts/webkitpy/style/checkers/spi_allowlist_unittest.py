
import unittest

from webkitpy.style.checkers.spi_allowlist import SPIAllowlistChecker


class SPIAllowlistCheckerStyleTestCase(unittest.TestCase):

    test_file_content = '''\
[networking."rdar://OOPS!"]
symbols = ["_allowed_symbol"]
classes = ["NSOopsShouldBeAllowed"]

[graphics]
"https://webkit.org/b/OOPS" = { selectors = ["initWithOops:andOOPS:"] }
'''

    expected_errors = {
        (1, 'build/spi-allowlist/bug-url', 5, 'Allowlist bug URL contains "OOPS"'),
        (6, 'build/spi-allowlist/bug-url', 5, 'Allowlist bug URL contains "OOPS"'),
    }

    def test_checker(self):
        errors = set()
        checker = SPIAllowlistChecker('AllowedSPI-test.toml', lambda *args: errors.add(args))

        checker.check(self.test_file_content.splitlines())
        self.assertEqual(self.expected_errors, errors)
