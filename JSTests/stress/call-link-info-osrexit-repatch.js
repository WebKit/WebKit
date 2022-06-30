//@ runFTLEager("--watchdog=1000", "--watchdog-exception-ok", "--usePollingTraps=true")
// FIXME: --usePollingTraps=true should be removed, https://bugs.webkit.org/show_bug.cgi?id=241927
function foo(a, b) {
    'use strict';
    if (a === 0) {
        return;
    }
    if (a === 0) {
        return foo(a + 0);
    }
    if (a === 0) {
        return foo(+a, 0);
    }
    return foo(b / 1, a, 0);
    0 === 0
}

foo(1, 5);
