function mathPowWrapper(x, y) {
    return Math.pow(x, y);
}
noInline(mathPowWrapper);

function testChangingMathPow() {
    var result = 0;
    for (var i = 0; i < 10000; ++i) {
        result += mathPowWrapper(3, 2);
    }
    Math.pow = function(a, b) { return a + b; }
    for (var i = 0; i < 10000; ++i) {
        result += mathPowWrapper(3, 2);
    }
    if (result !== 140000)
        throw "Result = " + result + ", expected 140000";
}
testChangingMathPow();