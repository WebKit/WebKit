//@ runFTLNoCJIT
var o1 = {
    i: 0,
    valueOf: function() { return this.i; }
};
var o2 = {
    i: 0,
    valueOf: function() { return this.i; }
};

result = 0;
function foo(a, b) {
    var result = 0;
    for (var j = 0; j < 10; j++) {
        if (a > b)
            result += a - b;
        else
            result += b - 1;
    }
    return result;
}
noInline(foo);

var iterations;
var expectedResult;
if (this.window) {
    // The layout test doesn't like too many iterations and may time out.
    iterations = 10000;
    expectedResult = 200020;
} else {
    iterations = 100000;
    expectedResult = 2000020;
}


for (var i = 0; i <= iterations; i++) {
    o1.i = i + 2;
    o2.i = i;
    result += foo(o1, o2);
}

if (result != expectedResult)
    throw "Bad result: " + result;
