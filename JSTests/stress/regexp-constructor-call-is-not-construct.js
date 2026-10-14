function shouldBe(actual, expected, what) {
    if (actual !== expected)
        throw new Error(what + ": bad value " + actual + ", expected " + expected);
}

function callWithCalleeAsThis(R, pattern, flags) { return R.call(R, pattern, flags); }
noInline(callWithCalleeAsThis);

function construct(R, pattern, flags) { return new R(pattern, flags); }
noInline(construct);

var re = /a/g;
for (var i = 0; i < testLoopCount; ++i) {
    shouldBe(callWithCalleeAsThis(RegExp, re, undefined), re, "RegExp(re, undefined)");

    var copy = construct(RegExp, re, undefined);
    shouldBe(copy !== re, true, "new RegExp(re, undefined) returns a new object");
    shouldBe(copy.source, "a", "new RegExp(re, undefined).source");
    shouldBe(copy.flags, "g", "new RegExp(re, undefined).flags");
}
