// debug-js -s --enableConcurrentJIT=false --thresholdForJITAfterWarmUp=1 --thresholdForOptimizeAfterWarmUp=2

function TestCase(a) {
    this.actual = 0.1;
}

var testcases = new Array();

for (var i=0; i < 3; i++){
    testcases[i] = new TestCase();
}
function test() {
    for ( tc=0; tc < testcases.length; tc++ ) {
        var v = testcases[tc];
        v.actual = v.actual;
        "" + testcases[tc].actual;
    }
    print("DONE");
}
test();