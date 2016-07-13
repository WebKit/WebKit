//@ defaultNoSamplingProfilerRun

// This should not crash
var boundFunc;

function testPromise(func) {
    var p1 = new Promise(func);
}
function promiseFunc(resolve, reject) {
    boundFunc();
}

boundFunc = testPromise.bind(null, promiseFunc);
boundFunc();
