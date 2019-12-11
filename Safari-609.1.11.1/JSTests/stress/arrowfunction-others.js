var testCase = function (actual, expected, message) {
    if (actual !== expected) {
        throw message + ". Expected '" + expected + "', but was '" + actual + "'";
    }
};

var simpleArrowFunction = () => {};

noInline(simpleArrowFunction);

function truthy() { return true; }
function falsey() { return false; }
noInline(truthy);
noInline(falsey);

for (var i=0;i<10000;i++) {
    testCase(Object.getPrototypeOf(simpleArrowFunction), Function.prototype, "Error: Not correct getPrototypeOf value for arrow function");

    testCase(simpleArrowFunction instanceof Function, true, "Error: Not correct result for instanceof method for arrow function");

    testCase(simpleArrowFunction.constructor == Function, true, "Error: Not correct result for constructor method of arrow functio   n");

    let a1 = truthy() ? ()=>1 : ()=>2;
    let a2 = falsey() ? ()=>2 : ()=>1;
    testCase(a1(), 1, "Should be 1");
    testCase(a2(), 1, "should be 1");
}
