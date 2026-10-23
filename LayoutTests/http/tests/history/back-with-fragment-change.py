#!/usr/bin/env python3

import sys
import time

# We intentionally want the page to load slowly (every time, hence no caching),
# so that when back-with-fragment-change-target.html calls history.back(), the
# traversal is still in flight when the window.location = '#foo' script runs.

time.sleep(2)

sys.stdout.write(
    'Cache-control: no-cache, no-store\r\n'
    'Pragma: no-cache\r\n'
    'Content-Type: text/html\r\n\r\n'
)

print('''<script>
if (window.testRunner) {
    testRunner.dumpBackForwardList();
    testRunner.dumpAsText();
    testRunner.waitUntilDone();
}

onload = function() {
    if (sessionStorage.didNavigate) {
        delete sessionStorage.didNavigate;
        document.getElementById('result').textContent = 'PASS: The fragment navigation did not cancel the traversal.';
        if (window.testRunner)
            testRunner.notifyDone();
        return;
    }

    // Change the location in a timeout to make sure it generates a history entry
    setTimeout(function() {
            window.location = 'resources/back-with-fragment-change-target.html'
            sessionStorage.didNavigate = true;
        }, 0);
}

// Make sure there's no page cache.
onunload = function() { };
</script>
<p>
Tests that a fragment navigation started while a history traversal is in flight does not cancel
the traversal. Per https://html.spec.whatwg.org/multipage/browsing-the-web.html#example-sync-navigation-steps-queue-jumping-basic,
the fragment navigation adds a new entry and the traversal then completes, so we should end up back
on this page with the fragment entry as forward history.</p>
<p id="result"></p>''')
