description(
"This tests that doing intrinsic function optimization does not result in this being lost entirely."
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
    
    // Create some bizzare object to prevent method_check optimizations, since those will result in
    // a failure while the function is still live.
    var object = {};
    object["a" + i] = i;
    object.stuff = functionToCall;
    object.f = 42;
    
    shouldBe("baz(object, " + i + ", " + (i * 2) + ")", "" + (offset + Math.max(i, i * 2)));
}

