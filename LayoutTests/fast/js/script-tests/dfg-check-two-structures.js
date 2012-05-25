description(
"This tests that a polymorphic structure check is actually executed."
);

function foo(o, p) {
    if (o == p)
        return o.f;
    else
        return 75;
}

var o1 = {f:42, g:43};
var o2 = {f:44};
var o3 = {e:45, f:46};

for (var i = 0; i < 200; ++i) {
    var o;
    var expected;
    if (i < 150) {
        if (i & 1) {
            o = o1;
            expected = 42;
        } else {
            o = o2;
            expected = 44;
        }
    } else {
        o = o3;
        expected = 46;
    }
    shouldBe("foo(o, o)", "" + expected);
}
