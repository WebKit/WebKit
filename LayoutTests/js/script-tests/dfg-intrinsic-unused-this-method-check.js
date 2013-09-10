description(
"This tests that doing intrinsic function optimization does not result in this being lost entirely, if method check optimizations succeed."
);

function bar(a, b) {
    return this.f + Math.max(a, b);
}

function baz(o, a, b) {
    return o.stuff(a, b);
}

var functionToCall = Math.max;
var offset = 0;
for (var i = 0; i < 1000; ++i) {
    if (i == 600) {
        functionToCall = bar;
        offset = 42;
    }
    
    var object = {};
    object.stuff = functionToCall;
    object.f = 42;
    
    shouldBe("baz(object, " + i + ", " + (i * 2) + ")", "" + (offset + Math.max(i, i * 2)));
}

