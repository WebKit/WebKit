load("./driver/driver.js");

function test(value)
{
    var ok = 0.5;
    ok += value;
    return ok;
}
noInline(test);
for (var i = 0; i < 1e4; ++i)
    test(1.2);
test(0.5);
var types = findTypeForExpression(test, "ok += value");
assert(types.instructionTypeSet.primitiveTypeNames.length === 2, "Primitive type names should two candidates.");
assert(types.instructionTypeSet.primitiveTypeNames.indexOf(T.Integer) !== -1, "Primitive type names should contain 'Integer'");
assert(types.instructionTypeSet.primitiveTypeNames.indexOf(T.Number) !== -1, "Primitive type names should contain 'Number'");
