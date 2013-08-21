description('Test for <a href="https://bugs.webkit.org/show_bug.cgi?id=119900">bug 119900</a>: Exception in global setter doesn\'t unwind correctly.');

debug("Passed if no assertion failure.");

this.__defineSetter__("setterThrowsException", function throwEmptyException(){ throw ""});

function callSetter() {
    setterThrowsException = 0;
}

for (var i = 0; i < 100; ++i) try { callSetter() } catch(e) { }
