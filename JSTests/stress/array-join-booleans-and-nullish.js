function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ', expected: ' + expected);
}

function join(array, separator) {
    return array.join(separator);
}
noInline(join);

var rope = "ro" + String.fromCharCode(112) + "e";
var cases = [
    [["a", true, 1, false, null, undefined, "b"], ",", "a,true,1,false,,,b"],
    [[true, false], "", "truefalse"],
    [[null, undefined], ",", ","],
    [[undefined], ",", ""],
    [["x", true, rope, -1], "--", "x--true--rope---1"],
    [["\u3042", false, null, 2], ",", "\u3042,false,,2"],
    [["a", true], "\u3042", "a\u3042true"],
    [[1, 2, false], ",", "1,2,false"],
    [["a", 1.5, true], ",", "a,1.5,true"],
    [[null, undefined, null], "", ""],
    [[null, "\u3042", undefined], "", "\u3042"],
    [[undefined, "a", null], "\u3042", "\u3042a\u3042"],
];

for (var i = 0; i < testLoopCount; ++i) {
    var [array, separator, expected] = cases[i % cases.length];
    shouldBe(join(array, separator), expected);
}

var holey = ["a", , true];
shouldBe(join(holey, ","), "a,,true");
Array.prototype[1] = "proto";
shouldBe(join(holey, ","), "a,proto,true");
shouldBe(join(["a", undefined, true], ","), "a,,true");
shouldBe(join(["a", null, true], ","), "a,,true");
delete Array.prototype[1];
