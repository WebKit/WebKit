function f() { return 40; }
function test({arg}) {
    function arg() { return 41; }
    return arg;
}
noInline(test);

for (var i = 0; i < 1000; i++) {
    if (test({arg: f})() !== 41)
        throw new Error("bad value");
}
