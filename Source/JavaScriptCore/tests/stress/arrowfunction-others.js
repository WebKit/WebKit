var testCase = function (actual, expected, message) {
  if (actual !== expected) {
    throw message + ". Expected '" + expected + "', but was '" + actual + "'";
  }
};

var simpleArrowFunction = () => {};

noInline(simpleArrowFunction);

for (var i=0;i<10000;i++) {
   testCase(Object.getPrototypeOf(simpleArrowFunction), Function.prototype, "Error: Not correct getPrototypeOf value for arrow function");

   testCase(simpleArrowFunction instanceof Function, true, "Error: Not correct result for instanceof method for arrow function");

   testCase(simpleArrowFunction.constructor == Function, true, "Error: Not correct result for constructor method of arrow functio   n");
}
