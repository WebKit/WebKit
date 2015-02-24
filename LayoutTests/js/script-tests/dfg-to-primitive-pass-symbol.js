description(
"Tests that op_to_primitive pass Symbol."
);

var flag = false;
debug("str concat generates op_to_primitive.");
function toPrimitiveTarget() {
    if (flag) {
        return Symbol('Cocoa');
    }
    return 'Cappuccino';
}
noInline(toPrimitiveTarget);

function doToPrimitive() {
    var value = toPrimitiveTarget();
    return value + "Cappuccino" + value;
}

while (!dfgCompiled({f:doToPrimitive})) {
    doToPrimitive();
}
flag = true;
shouldThrow("doToPrimitive()", "'TypeError: Type error'");
