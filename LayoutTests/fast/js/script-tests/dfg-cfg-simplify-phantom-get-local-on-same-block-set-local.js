description(
"Tests that attempts by the DFG simplification to short-circuit a Phantom to a GetLocal on a variable that is SetLocal'd in the same block, and where the predecessor block(s) make no mention of that variable, do not result in crashes."
);

function baz() {
    // Do something that prevents inlining.
    return function() { }
}

function stuff(z) { }

function foo(x, y) {
    var a = arguments; // Force arguments to be captured, so that x is captured.
    baz();
    var z = x;
    stuff(z); // Force a Flush, and then a Phantom on the GetLocal of x.
    return 42;
}

var o = {
    g: function(x) { }
};

function thingy(o) {
    var p = {};
    var result;
    // Trick to delay control flow graph simplification until after the flush of x above gets turned into a phantom.
    if (o.g)
        p.f = true;
    if (p.f) {
        // Basic block that stores to x in foo(), which is a captured variable, with
        // the predecessor block making no mention of x.
        result = foo("hello", 2);
    }
    return result;
}

for (var i = 0; i < 200; ++i)
    shouldBe("thingy(o)", "42");

