description(
"Tests what happens when CFG simplification leads to the elimination of a set local that had a type check, and then we branch on the variable not being null."
);

function foo(o) {
    var x;
    if (o.f)
        x = o.g;
    else
        x = o.h;
    if (x != null)
        return x - 1;
    else
        return x;
}

for (var i = 0; i < 500; ++i) {
    var o = {f:foo};
    var expected;
    if (i < 450) {
        o.g = i;
        expected = "" + (i - 1);
    } else {
        o.g = null;
        expected = "null";
    }
    shouldBe("foo(o)", expected);
}

