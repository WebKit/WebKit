description(
"Tests what happens when CFG simplification leads to the elimination of a set local that had a type check, and then we do a typeof on the value, which can be constant folded if the type check stays."
);

function foo(o) {
    var x;
    if (o.f)
        x = o.g;
    else
        x = o.h;
    return [typeof x, x - 1];
}

for (var i = 0; i < 500; ++i) {
    var o = {f:foo};
    var expectedFirst;
    var expectedSecond;
    if (i < 450) {
        o.g = i;
        expectedFirst = "\"number\"";
        expectedSecond = "" + (i - 1);
    } else {
        o.g = "42";
        expectedFirst = "\"string\"";
        expectedSecond = "41";
    }
    var result = foo(o);
    shouldBe("result[0]", expectedFirst);
    shouldBe("result[1]", expectedSecond);
}

