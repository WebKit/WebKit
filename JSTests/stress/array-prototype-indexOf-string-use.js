function sameValue(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

function test(array, value) {
    return array.indexOf(value);
}
noInline(test);

var array = [1, 2, 3.4, "foo", 5, 6, 6, 4];
for (let i = 0; i < testLoopCount; i++) {
    sameValue(test(array, "foo"), 3);
    sameValue(test(array, "bar"), -1);
}
