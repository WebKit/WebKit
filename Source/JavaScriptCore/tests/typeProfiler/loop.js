load("./driver/driver.js");

function testForIn(x) {
    for (var arg1 in x)
        x;

    for (arg2 in x)
        x; 

    for ({x: arg3} in x)
        x;

    for (var {x: arg4} in x)
        x;
}

function testForOf(x) {
    for (var arg1 of x)
        x;

    for (arg2 of x)
        x; 

    for ({x: arg3} of x) 
        x;
    for (var {x: arg4} of x) 
        x;
}

testForIn([1])
var types = findTypeForExpression(testForIn, "arg1"); 
assert(types.instructionTypeSet.primitiveTypeNames.indexOf(T.String) !== -1, "Primitive type names should contain 'String'");
types = findTypeForExpression(testForIn, "arg2"); 
assert(types.instructionTypeSet.primitiveTypeNames.indexOf(T.String) !== -1, "Primitive type names should contain 'String'");
types = findTypeForExpression(testForIn, "arg3");
assert(types.instructionTypeSet.primitiveTypeNames.indexOf(T.Undefined) !== -1, "Primitive type names should contain 'Undefined'"); 
types = findTypeForExpression(testForIn, "arg4");
assert(types.instructionTypeSet.primitiveTypeNames.indexOf(T.Undefined) !== -1, "Primitive type names should contain 'Undefined'"); 

testForOf([1])
types = findTypeForExpression(testForOf, "arg1"); 
assert(types.instructionTypeSet.primitiveTypeNames.indexOf(T.Integer) !== -1, "Primitive type names should contain 'Integer'");
types = findTypeForExpression(testForOf, "arg2"); 
assert(types.instructionTypeSet.primitiveTypeNames.indexOf(T.Integer) !== -1, "Primitive type names should contain 'Integer'");
types = findTypeForExpression(testForOf, "arg3");
assert(types.instructionTypeSet.primitiveTypeNames.indexOf(T.Undefined) !== -1, "Primitive type names should contain 'Undefined'"); 
types = findTypeForExpression(testForOf, "arg4");
assert(types.instructionTypeSet.primitiveTypeNames.indexOf(T.Undefined) !== -1, "Primitive type names should contain 'Undefined'"); 
testForOf([{x:29}])
types = findTypeForExpression(testForOf, "arg1"); 
assert(types.instructionTypeSet.structures[0].fields.indexOf("x") !== -1, "variable 'arg1' should have field 'x'");
types = findTypeForExpression(testForOf, "arg2");
assert(types.instructionTypeSet.structures[0].fields.indexOf("x") !== -1, "variable 'arg2' should have field 'x'");
types = findTypeForExpression(testForOf, "arg3");
assert(types.instructionTypeSet.primitiveTypeNames.indexOf(T.Integer) !== -1, "Primitive type names should contain 'Integer'"); 
types = findTypeForExpression(testForOf, "arg4");
assert(types.instructionTypeSet.primitiveTypeNames.indexOf(T.Integer) !== -1, "Primitive type names should contain 'Integer'"); 
