description(
"Tests that we use Phantom on the base of put_by_base correctly."
);

function foo(o) {
    var x = o;
    var y = o.f;
    if (y) {
        o.g.h;
        return !x;
    }
    
    // Do things to ensure that the structure check on o is not hoisted.
    return o + o + o + o + o;
}

for (var i = 0; i < 200; ++i)
    shouldBe("foo(i < 190 ? {f:42, g:{h:3}} : {f:42, g:{}})", "false");

