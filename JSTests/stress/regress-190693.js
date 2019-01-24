// Reduced and tweaked code from const-semantics.js to reproduce https://bugs.webkit.org/show_bug.cgi?id=190693 easily.
"use strict";
function truth() {
    return true;
}
noInline(truth);

function assert(cond) {
    if (!cond)
        throw new Error("broke assertion");
}
noInline(assert);
function shouldThrowInvalidConstAssignment(f) {
    var threw = false;
    try {
        f();
    } catch(e) {
        if (e.name.indexOf("TypeError") !== -1 && e.message.indexOf("readonly") !== -1)
            threw = true;
    }
    assert(threw);
}
noInline(shouldThrowInvalidConstAssignment);


// ========== tests below ===========

const NUM_LOOPS = 6000;

;(function() {
    function taz() {
        const x = 20;
        shouldThrowInvalidConstAssignment(function() { x = 20; });
        assert(x === 20);
        shouldThrowInvalidConstAssignment(function() { x += 20; });
        assert(x === 20);
        shouldThrowInvalidConstAssignment(function() { x -= 20; });
        assert(x === 20);
        shouldThrowInvalidConstAssignment(function() { x *= 20; });
        assert(x === 20);
        shouldThrowInvalidConstAssignment(function() { x /= 20; });
        assert(x === 20);
        shouldThrowInvalidConstAssignment(function() { x >>= 20; });
        assert(x === 20);
        shouldThrowInvalidConstAssignment(function() { x <<= 20; });
        assert(x === 20);
        shouldThrowInvalidConstAssignment(function() { x ^= 20; });
        assert(x === 20);
        shouldThrowInvalidConstAssignment(function() { x++; });
        assert(x === 20);
        shouldThrowInvalidConstAssignment(function() { x--; });
        assert(x === 20);
        shouldThrowInvalidConstAssignment(function() { ++x; });
        assert(x === 20);
        shouldThrowInvalidConstAssignment(function() { --x; });
        assert(x === 20);
    }
    for (var i = 0; i < NUM_LOOPS; i++) {
        taz();
    }
})();

for(var i = 0; i < 1e6; ++i);
