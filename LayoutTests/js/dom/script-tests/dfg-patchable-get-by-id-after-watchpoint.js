description(
"This tests that a patchable GetById right after a watchpoint has the appropriate nop padding."
);

function foo(o, p) {
    var a = p.f;
    var b = o.f; // Watchpoint.
    var c = p.g; // Patchable GetById.
    return b(a + c);
}

function O() {
}

O.prototype.f = function(x) { return x + 1; };

var o = new O();

function P1() {
}

P1.prototype.g = 42;

function P2() {
}

P2.prototype.g = 24;

var p1 = new P1();
var p2 = new P2();

p1.f = 1;
p2.f = 2;

for (var i = 0; i < 200; ++i) {
    var p = (i % 2) ? p1 : p2;
    var expected = (i % 2) ? 44 : 27;
    if (i == 150) {
        // Cause first the watchpoint on o.f to fire, and then the GetById
        // to be reset.
        O.prototype.g = 57; // Fire the watchpoint.
        P1.prototype.h = 58; // Reset the GetById.
        P2.prototype.h = 59; // Not necessary, but what the heck - this resets the GetById even more.
    }
    shouldBe("foo(o, p)", "" + expected);
}

