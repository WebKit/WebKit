load("./driver/driver.js");

let changeFoo;
let tdzError;
let scoping;
let scoping2;
function noop(){}
function arr() {
    return [1, 1.5, "hello", {}];
}

function wrapper() {

let foo=20;
changeFoo = function(arg) { foo = arg; }

scoping = function ()
{
    let x = "hello";
    if (true) {
        let x = 20;
        const y = true;
        x = "h"
    }
    noop(x);
}

scoping2 = function() 
{
    for (const item of arr()) {
        noop(item);
    }
}

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


scoping();
types = findTypeForExpression(scoping, "x = 20");
assert(types.instructionTypeSet.primitiveTypeNames.indexOf(T.Integer) !== -1, "Primitive type names should contain 'Integer'");
assert(types.instructionTypeSet.primitiveTypeNames.length === 1, "Primitive type names should contain only one item on the instruction");
assert(types.globalTypeSet.primitiveTypeNames.indexOf(T.Integer) !== -1, "Primitive type names should contain 'Integer'");
assert(types.globalTypeSet.primitiveTypeNames.length === 2, "Primitive type names should contain two items: [String, Integer]");

types = findTypeForExpression(scoping, "x)");
assert(types.globalTypeSet.primitiveTypeNames.indexOf(T.String) !== -1, "Global primitive type names should have string.");
assert(types.instructionTypeSet.primitiveTypeNames.indexOf(T.String) !== -1, "Global primitive type names should have string.");

types = findTypeForExpression(scoping, "y = true");
assert(types.globalTypeSet.primitiveTypeNames.indexOf(T.Boolean) !== -1, "Global primitive type names should have boolean.");
assert(types.globalTypeSet.primitiveTypeNames.length === 1, "type only have one item.");
assert(types.instructionTypeSet.primitiveTypeNames.indexOf(T.Boolean) !== -1, "Global primitive type names should have boolean.");
assert(types.instructionTypeSet.primitiveTypeNames.length === 1, "type only have one item.");


scoping2();
types = findTypeForExpression(scoping2, "item)");
assert(types.instructionTypeSet.primitiveTypeNames.indexOf(T.Integer) !== -1, "Primitive type names should contain 'Integer'");
assert(types.instructionTypeSet.primitiveTypeNames.indexOf(T.Number) !== -1, "Primitive type names should contain 'Number'");
assert(types.instructionTypeSet.primitiveTypeNames.indexOf(T.String) !== -1, "Primitive type names should contain 'String'");
assert(types.instructionTypeSet.structures.length === 1, "should have one structure");
assert(types.instructionTypeSet.structures[0].constructorName === "Object", "Should be object");
assert(types.instructionTypeSet.structures[0].fields.length === 0, "Should have no fields");
