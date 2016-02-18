//@ noNoLLIntRunLayoutTest if $architecture == "arm" and $hostOS == "darwin"

description(
"Tests that performing an OSR entry into a loop with a hoisted structure check, where the loop may clobber the world, works."
);

function foo(o, n) {
    var result = 0;
    for (var i = 0; i < n; ++i) {
        result += o.f;
        result += o.g(i);
        if (i > 1)
            result += o.i;
        if (i > 2)
            result += o.j;
        o = o.h(i);
        result += o.g(i);
    }
    return result;
}

function bar(i) {
    var str = "var x" + i + " = 42; x" + i;
    return eval(str);
}

var counter = 0;
function baz(i) {
    var str = "var x" + i + " = 42; x" + i;
    eval(str);
    if (i == 1100)
        return {g:bar, i:3, j:4, h:baz, f:2 + counter++};
    return this;
}

var object = {f:1, g:bar, h:baz, i:2, j:3};
shouldBe("foo(object, 10000)", "926684");


