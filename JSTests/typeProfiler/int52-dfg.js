load("./driver/driver.js");

function test()
{
    var ok = 0;
    for (var i = 0; i < 1e4; ++i) {
        // Int52. ProfileType should not use AnyIntUse edge in 32bit environment.
        // If 32bit uses AnyIntUse, it leads crashing.
        ok += 0xfffffffff;
    }
    return ok;
}
test();

var types = findTypeForExpression(test, "ok += 0x");
assert(types.instructionTypeSet.primitiveTypeNames.length === 1, "Primitive type names should one candidate.");
assert(types.instructionTypeSet.primitiveTypeNames.indexOf(T.Integer) !== -1, "Primitive type names should contain 'Integer'");
