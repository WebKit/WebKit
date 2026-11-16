// Regression test for op_call_varargs_with_spread argument-elimination: a fused varargs-spread call
// whose sibling spread operand escapes through a later use must materialize ALL its spreads (no FTL crash).

function assert(b, msg) {
    if (!b)
        throw new Error("Bad assertion: " + msg);
}
noInline(assert);

var sink;

function shape() {
    var s = "";
    for (var i = 0; i < arguments.length; ++i)
        s += arguments[i] + ",";
    return s;
}
noInline(shape);

function callEscapesRestAfter(...a) {
    var r = shape(...a, ...[8, 9]);
    sink = a;
    return r;
}
noInline(callEscapesRestAfter);

function callTwoConstArrays() {
    var first = [1, 2];
    var r = shape(...first, ...[3, 4]);
    sink = function () { return first; };
    return r;
}
noInline(callTwoConstArrays);

for (var i = 0; i < testLoopCount; ++i) {
    assert(callEscapesRestAfter(1, 2, 3) === "1,2,3,8,9,", "callEscapesRestAfter " + callEscapesRestAfter(1, 2, 3));
    assert(callTwoConstArrays() === "1,2,3,4,", "callTwoConstArrays " + callTwoConstArrays());
}

function target() {
    var s = "";
    for (var i = 0; i < arguments.length; ++i)
        s += arguments[i] + ",";
    return s;
}
noInline(target);

function inlinedSpreadEscape(...rest) {
    var r = target(...rest, ...[100, 200]);
    sink = rest;
    return r;
}
function driver() { return inlinedSpreadEscape(5, 6); }
noInline(driver);

for (var i = 0; i < testLoopCount; ++i)
    assert(driver() === "5,6,100,200,", "driver " + driver());
