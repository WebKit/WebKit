load("./driver/driver.js");

function tierUpToDFG(func, arg) 
{
    for (var i = 0; i < 1000; i++)
        func(arg);
}

var types = [T.Boolean, T.Integer, T.Null, T.Number, T.String, T.Undefined];
var values = [true, 1, null, 1.1, "hello", undefined];
var funcs = [function(x) { return x; }, function(x) { return x; }, function(x) { return x; }, 
    function(x) { return x; }, function(x) { return x; }, function(x) { return x; }]

assert(types.length === values.length && values.length === funcs.length);

// Make sure baseline DFG type check optimizations work.
// Tier up each function on a different type, then call it with all other primitive types to make sure they're recorded.
for (var i = 0; i < types.length; i++) {
    var func = funcs[i];
    var type = types[i];
    var value = values[i];
    noInline(func);
    tierUpToDFG(func, value);
    var typeInfo = findTypeForExpression(func, "x"); 
    assert(typeInfo.instructionTypeSet.primitiveTypeNames.length === 1, "Only one primitive type should have been seen so far.");
    assert(typeInfo.instructionTypeSet.primitiveTypeNames.indexOf(type) !== -1, "Should have been optimized on type: " + type);
    for (var j = 0; j < types.length; j++) {
        if (type === T.Number && types[j] === T.Integer) {
            // If we optimize on type Number, but we pass in an Integer, our typecheck will still pass, so we wont directly get
            // type Integer because T.Integer is implied to be contained in T.Number.
            continue; 
        }

        func(values[j]);
        typeInfo = findTypeForExpression(func, "x");
        assert(typeInfo.instructionTypeSet.primitiveTypeNames.indexOf(types[j]) !== -1, "Should have seen type: " + types[j] + " " + i + "," + j);
    }
}

function testStrucure(obj) { return obj; }
var obj = {x: 20};
tierUpToDFG(testStrucure, obj);
var types = findTypeForExpression(testStrucure, "obj");
assert(types.instructionTypeSet.structures.length === 1, "variable 'test' should have exactly structure");
assert(types.instructionTypeSet.structures[0].fields.length === 1, "variable 'test' should have exactly ONE field");
assert(types.instructionTypeSet.structures[0].fields.indexOf("x") !== -1, "variable 'test' should have field 'x'");

testStrucure({x:20, y: 40})
types = findTypeForExpression(testStrucure, "obj");
assert(types.instructionTypeSet.structures.length === 1, "variable 'test' should have still have exactly one structure");
assert(types.instructionTypeSet.structures[0].fields.indexOf("x") !== -1, "variable 'test' should have field 'x'");
assert(types.instructionTypeSet.structures[0].optionalFields.indexOf("y") !== -1, "variable 'test' should have field optional field 'y'");
