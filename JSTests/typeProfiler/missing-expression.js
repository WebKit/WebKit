var findTypeForExpression = $vm.findTypeForExpression;
var returnTypeFor = $vm.returnTypeFor;

load("./driver/driver.js");

function foo() {
    var x = 1;
}
foo();

var types = findTypeForExpression(foo, "missing");
assert(types === null, "missing expression should not crash");

types = findTypeForExpression(foo, "x = 1");
assert(types.instructionTypeSet.primitiveTypeNames.indexOf(T.Integer) !== -1, "known expression should still work");

function unused() {
    return 1;
}
var unusedTypes = returnTypeFor(unused);
assert(unusedTypes.instructionTypeSet === null, "unexecuted function should not crash");
