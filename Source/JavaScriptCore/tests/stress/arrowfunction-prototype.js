var testCase = function (actual, expected, message) {
  if (actual !== expected) {
    throw message + ". Expected '" + expected + "', but was '" + actual + "'";
  }
};

var af1 = () =>  {};
var af2 = (a) => {a + 1};
var af3 = (x) =>  x + 1;

noInline(af1);
noInline(af2);
noInline(af3);

for (var i = 0; i < 10000; ++i) {
  testCase(typeof af1.prototype, 'undefined', "Error: Not correct result for prototype of arrow function #1");
  testCase(typeof af2.prototype, 'undefined', "Error: Not correct result for prototype of arrow function #2");
  testCase(typeof af3.prototype, 'undefined', "Error: Not correct result for prototype of arrow function #5");
  testCase(af1.hasOwnProperty("prototype"), false, "Error: Not correct result for prototype of arrow function #3");
  testCase(af2.hasOwnProperty("prototype"), false, "Error: Not correct result for prototype of arrow function #4");
  testCase(af3.hasOwnProperty("prototype"), false, "Error: Not correct result for prototype of arrow function #6");
}
