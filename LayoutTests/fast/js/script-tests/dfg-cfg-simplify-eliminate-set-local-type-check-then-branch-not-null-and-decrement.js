description(
"Tests what happens when CFG simplification leads to the elimination of a set local that had a type check, and then we branch on the variable not being null while decrementing the variable in the same basic block."
);

function foo(o) {
    var x;
    if (o.f)
        x = o.g;
    else
        x = o.h;
    var y = x != null;
    x--;
    if (y)
        return y;
    else
        return false;
}

for (var i = 0; i < 500; ++i) {
    var o = {f:foo};
    var expected;
    if (i < 450) {
        o.g = i;
        expected = "true";
    } else {
        o.g = null;
        expected = "false";
    }
    shouldBe("foo(o)", expected);
}


