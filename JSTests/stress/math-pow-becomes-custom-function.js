function mathPowWrapper(x, y) {
    return Math.pow(x, y);
}
noInline(mathPowWrapper);

function testChangingMathPow() {
    var result = 0;
    for (var i = 0; i < testLoopCount; ++i) {
        result += mathPowWrapper(3, 2);
    }
    Math.pow = function(a, b) { return a + b; }
    for (var i = 0; i < testLoopCount; ++i) {
        result += mathPowWrapper(3, 2);
    }
    if (result !== (9 + 5) * testLoopCount)
        throw `Result = ${result}, expected ${(9 + 5) * testLoopCount}`;
}
testChangingMathPow();