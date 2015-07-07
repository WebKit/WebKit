description(
"Tests that we don't crash if a watchpoint on prototype access stubs is fired."
);

function A() { }
function B() { }

A.prototype.f = 42;
B.prototype.f = 43;


function foo(o) {
    return o.f;
}

for (var i = 0; i < 200; ++i) {
    if (i == 150)
        A.prototype.g = 63;
    shouldBe("foo((i % 2) ? new A() : new B())", "" + ((i % 2) ? 42 : 43));
}
