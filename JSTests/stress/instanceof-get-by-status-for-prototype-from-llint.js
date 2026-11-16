function assert(b) {
    if (!b)
        throw new Error("Bad assertion");
}

globalThis.testLoopCount ??= 1000
function test(f) {
    for (let i = 0; i < testLoopCount; i++)
        f();
}

var EQ = function () {
    function EQ2() { };
    EQ2.value = new EQ2();
    return EQ2;
}();

test(function () {
    let a = EQ.value;
    assert(a instanceof EQ);
});
