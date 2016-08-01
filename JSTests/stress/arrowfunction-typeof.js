var testCase = function (actual, expected, message) {
  if (actual !== expected) {
    throw message + ". Expected '" + expected + "', but was '" + actual + "'";
  }
};

var af1 = () => {};
var af2 = (a) => {a + 1};

noInline(af1);
noInline(af2);

for (var i = 0; i < 10000; ++i) {
  testCase(typeof af1, "function", "Error: Not correct type of the arrow function #1");
  testCase(typeof af2, "function", "Error: Not correct type of the arrow function #2");

//Fixme: Some bug in inlining typeof with following run parameters ftl-no-cjit-no-inline-validate
// --useFTLJIT\=true --useFunctionDotArguments\=true --useConcurrentJIT=false --thresholdForJITAfterWarmUp=100  --validateGraph=true --maximumInliningDepth=1
//
// for (var i = 0; i < 10000; ++i)  {
//   if (typeof (function () {}) !== 'function')
//       throw 'Wrong type';
// }
//  testCase(typeof ()=>{}, "function", "Error: Not correct type of the arrow function #3-" + i);

//  testCase(typeof ((b) => {b + 1}), "function", "Error: Not correct type of the arrow function #4");
}
