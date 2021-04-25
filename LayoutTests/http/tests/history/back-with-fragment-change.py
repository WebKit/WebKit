#!/usr/bin/env python3

import sys
import time

# We intentionally want the page to load slowly (every time, hence no caching), 
# so that when back-with-fragment-change-target.html calls history.back(), the
# load is provisional for a while (long enough for the window.location = '#foo'
# script to run and stop that load).

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
        console.log('Should not have ended up back at the test start page');
        delete sessionStorage.didNavigate;
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
Tests that a history navigation that is aborted by a fragment change doesn't
update the provisional history item. This test relies on
<code>testRunner.dumpBackForwardList</code> to verify correctness
and thus can only be run via DumpRenderTree.</p>''')