description(
"This tests that a structure clobber in a basic block that does not touch a live variable causes that variable's structure to be correctly clobbered."
);

var clobberedObject;

function bar() {
    if (!clobberedObject)
        return;
    delete clobberedObject.f;
    clobberedObject.g = 42;
}

function foo(p, o_) {
    var o = o_.f; // Force this block to have a SetLocal.
    var x = o.f;
    if (p)
        bar();
    return x + o.f;
}

var expected = 2;

for (var i = 0; i < 200; ++i) {
    var object = {f:1};
    var predicate = true;
    if (i >= 150) {
        clobberedObject = object;
        expected = 0/0; // "NaN"
    }
    shouldBe("foo(predicate, {f:object})", "" + expected);
}

