var didThrow = false;
try {
    (function() {
        for (var i = 0; i < testLoopCount; ++i) { }
        throw 42;
    })();
} catch (e) {
    if (e != 42)
        throw "Error: bad result: " + e;
    didThrow = true;
}
if (!didThrow)
    throw "Error: should have thrown but didn't.";
