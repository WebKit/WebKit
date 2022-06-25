#!/usr/bin/env python3

import os
import sys

cache_control = os.environ.get('HTTP_CACHE_CONTROL', '')
pragma = os.environ.get('HTTP_PRAGMA', '')

sys.stdout.write('Content-Type: text/html\r\n\r\n')

if cache_control.lower() == 'no-cache' or pragma.lower() == 'no-cache':
    sys.stdout.write(
        '<p>Got a no-cache directive; FAILURE!</p>'
        '<script>if (window.testRunner) { testRunner.notifyDone(); }</script>'
    )
elif cache_control.lower() == 'max-age=0':
    sys.stdout.write(
        '<p>SUCCESS</p>'
        '<script>if (window.testRunner) { testRunner.notifyDone(); }</script>'
    )
else:
    sys.stdout.write(
        '<body onload="window.location.reload();">'
        '<p>No cache control headers, reloading...</p>'
        '<script>if (window.testRunner) { testRunner.waitUntilDone(); }</script>'
        '<script>function test() {window.location.reload();}</script>'
    )

sys.stdout.write(
    '<script>if (window.testRunner) { testRunner.dumpAsText(); }</script>'
    '<p>Test for <a href="http://bugzilla.opendarwin.org/show_bug.cgi?id=5499">bug 5499</a>: Page reload does not send any cache control headers.</p>'
)