#!/usr/bin/env python3
import sys
import time

# Delay must be longer than 500ms (the initial setTimeout in the HTML
# test) so the grandchild has not yet arrived when the PI injection
# triggers compilation, but shorter than 3500ms (500ms + 3000ms total
# test timeout). After the delay, this response arrives and
# XSLImportRule::setXSLStyleSheet() -> parseString() runs on the
# grandchild -- which, without the fix, dereferences the freed
# m_stylesheetDoc of the parent (child.xsl).
time.sleep(1)

sys.stdout.write('Content-Type: text/xml\r\n\r\n')
sys.stdout.write('<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform"/>')
