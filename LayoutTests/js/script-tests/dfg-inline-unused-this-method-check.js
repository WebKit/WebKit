description(
"This tests that inlining a function that does not use this does not result in this being lost entirely, if we succeed in doing method check optimizations."
);

function foo(a, b) {
    return a + b;
}

function bar(a, b) {
    return this.f + a + b;
}

function baz(o, a, b) {
    return o.stuff(a, b);
}

var functionToCall = foo;
var offset = 0;
for (var i = 0; i < 1000; ++i) {
    if (i == 600) {
        functionToCall = bar;
        offset = 42;
    }
    
    var object = {};
    object.stuff = functionToCall;
    object.f = 42;
    
    shouldBe("baz(object, " + i + ", " + (i * 2) + ")", "" + (offset + i + i * 2));
}

