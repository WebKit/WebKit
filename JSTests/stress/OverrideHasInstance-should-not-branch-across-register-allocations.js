//@ runDefault

// This test should not crash.

delete this.Function;

var test = function() { 
    Math.cos("0" instanceof arguments)
}

for (var k = 0; k < testLoopCount; ++k) {
    try {
        test();
    } catch (e) {
    }
}
