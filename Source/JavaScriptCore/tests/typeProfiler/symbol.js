load("./driver/driver.js");

function wrapper() {
    var s1 = Symbol();

    var sCaptured = Symbol();
    function foo() {
        sCaptured = null;
    }
    foo();

    function bar(s3) { return s3; }
    for (var i = 0; i < 1000; i++)
        bar(i)
    bar(Symbol())

    function baz(s4) { return s4; }
    for (var i = 0; i < 1000; i++)
        baz(Symbol())
    baz("hello")
}

wrapper();

// ====== End test cases ======

var types = findTypeForExpression(wrapper, "s1"); 
assert(types.instructionTypeSet.primitiveTypeNames.length === 1, "Primitive type names should contain one type");
assert(types.instructionTypeSet.primitiveTypeNames.indexOf(T.Symbol) !== -1, "Primitive type names should contain 'Symbol'");

types = findTypeForExpression(wrapper, "sCaptured");
assert(types.globalTypeSet.primitiveTypeNames.length === 2, "Primitive type names should contain two items.");
assert(types.globalTypeSet.primitiveTypeNames.indexOf(T.Symbol) !== -1, "Primitive type names should contain 'Symbol'");
assert(types.globalTypeSet.primitiveTypeNames.indexOf(T.Null) !== -1, "Primitive type names should contain 'Null'");

types = findTypeForExpression(wrapper, "s3");
assert(types.instructionTypeSet.primitiveTypeNames.length === 2, "Primitive type names should contain 2 items");
assert(types.instructionTypeSet.primitiveTypeNames.indexOf(T.Integer) !== -1, "Primitive type names should contain 'Integer'");
assert(types.instructionTypeSet.primitiveTypeNames.indexOf(T.Symbol) !== -1, "Primitive type names should contain 'Symbol'");

types = findTypeForExpression(wrapper, "s4");
assert(types.instructionTypeSet.primitiveTypeNames.length === 2, "Primitive type names should contain 2 items");
assert(types.instructionTypeSet.primitiveTypeNames.indexOf(T.String) !== -1, "Primitive type names should contain 'String'");
assert(types.instructionTypeSet.primitiveTypeNames.indexOf(T.Symbol) !== -1, "Primitive type names should contain 'Symbol'");
