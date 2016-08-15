load("./driver/driver.js");

var changeFoo;
function wrapper() {

var foo=20;
changeFoo = function(arg) { foo = arg; }

}
wrapper();

// ====== End test cases ======

var types = findTypeForExpression(wrapper, "foo=20;"); 
assert(types.instructionTypeSet.primitiveTypeNames.indexOf(T.Integer) !== -1, "Primitive type names should contain 'Integer'");
assert(types.globalTypeSet.primitiveTypeNames.indexOf(T.Integer) !== -1, "Primitive type names should contain 'Integer'");
assert(types.globalTypeSet.primitiveTypeNames.length === 1, "Primitive type names should contain exactly only one item globally");
assert(types.instructionTypeSet.primitiveTypeNames.length === 1, "Primitive type names should contain exactly only one item on the instruction");
assert(types.globalTypeSet.displayTypeName === T.Integer, "global display name should be Integer");
assert(types.instructionTypeSet.displayTypeName === T.Integer, "instruction display name should be Integer");

changeFoo(20.5);
types = findTypeForExpression(wrapper, "foo=20;"); 
assert(types.instructionTypeSet.primitiveTypeNames.indexOf(T.Integer) !== -1, "Primitive type names should contain 'Integer'");
assert(types.instructionTypeSet.primitiveTypeNames.length === 1, "Primitive type names should contain STILL only contain exactly one item on the instruction");
assert(types.globalTypeSet.primitiveTypeNames.indexOf(T.Integer) !== -1, "Global primitive type names should now still contain 'Integer'");
assert(types.globalTypeSet.primitiveTypeNames.indexOf(T.Number) !== -1, "Global primitive type names should now contain 'Number'");
assert(types.globalTypeSet.primitiveTypeNames.length === 2, "Global primitive type names should contain exactly two items globally");
assert(types.globalTypeSet.displayTypeName === T.Number, "global display name should be Number");

changeFoo(null);

