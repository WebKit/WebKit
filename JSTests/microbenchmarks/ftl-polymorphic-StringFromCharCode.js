//@ runFTLNoCJIT
var o1 = {
    i: 65,
    valueOf: function() { return this.i; }
};

result = 0;
function foo(a) {
    var result = String.fromCharCode(a);

    // Busy work just to allow the DFG and FTL to optimize this. If the above causes
    // us to speculation fail out to the baseline, this busy work will take a lot longer
    // to run.
    // This loop below also gets the DFG to compile this function sooner.
    var count = 0;
    for (var i = 0; i < 1000; i++)
        count++;
    return result + count;
}
noInline(foo);

var iterations;
var expectedResult;
if (this.window) {
    // The layout test doesn't like too many iterations and may time out.
    iterations = 10000;
    expectedResult = 10001;
} else {
    iterations = 100000;
    expectedResult = 100001;
}


for (var i = 0; i <= iterations; i++) {
    var resultStr;
    if (i % 2 == 2)
        resultStr = foo('65');
    else if (i % 2 == 1)
        resultStr = foo(o1);
    else
        resultStr = foo(65);
    if (resultStr == 'A1000')
        result++;
}

if (result != expectedResult)
    throw "Bad result: " + result;
