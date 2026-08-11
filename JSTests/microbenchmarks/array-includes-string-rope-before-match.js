function makeRope(a, b) {
    return a + b;
}
noInline(makeRope);

var array = [
    undefined, undefined, undefined, undefined,
    undefined, undefined, undefined, undefined,
    makeRope('he', 'y'),
    '',
];

function test(array) {
    return array.includes('');
}
noInline(test);

for (var i = 0; i < 1e6; ++i) {
    if (!test(array))
        throw new Error("bad");
}
