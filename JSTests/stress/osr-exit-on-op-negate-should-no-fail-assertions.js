//@ runFTLNoCJIT
// This test passes if it does not crash or fail any assertions.

function inlineable(x) {
    return -x;
}

function test(y) {
    var results = [];
    for (var j = 0; j < 300; j++) {
        var k = j % y.length;
        try {
            results.push(inlineable(y[k]));
        } catch (e) {
        }
    }
}
noInline(test);

for (var i = 0; i < 1000; i++) {
    test([false, -Infinity, Infinity, 0x50505050, undefined]);
}
