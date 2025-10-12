
import re

re_table = re.compile(r'^\s*\[\s*(\w+|"[^"]+")(?:\.(\w+|"[^"]+"))*\s*\]\s*$')
re_inline_table = re.compile(r'(\w+|"[^"]+")\s*=\s*{')
re_url = re.compile(r'(http|https|radar|rdar)://.+')


class SPIAllowlistChecker(object):

    """Processes AllowedSPI*.toml files to enforce SPI allowlisting
    conventions."""

    def __init__(self, file_path, handle_style_error):
        self.file_path = file_path
        self.handle_style_error = handle_style_error

    def check(self, lines):
        self._check_oops_in_bug_url(lines)

    def _check_oops_in_bug_url(self, lines):
        for line_number, line in enumerate(lines):
            match = re_table.match(line) or re_inline_table.search(line)
            if not match:
                continue
            for key in match.groups(''):
                key = key.strip('"')
                if re_url.match(key) and 'oops' in key.lower():
                    self.handle_style_error(line_number + 1,
                                            'build/spi-allowlist/bug-url', 5,
                                            'Allowlist bug URL contains "OOPS"')
