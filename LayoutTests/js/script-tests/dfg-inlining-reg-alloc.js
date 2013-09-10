description(
"This tests that register allocation still works under register pressure induced by inlining, out-of-line function calls (i.e. unconditional register flushing), and slow paths for object creation (i.e. conditional register flushing)."
);

// Inlineable constructor.
function foo(a, b, c) {
    this.a = a;
    this.b = b;
    this.c = c;
}

// Non-inlineable function. This relies on a size limit for inlining, but still
// produces integers. It also relies on the VM not reasoning about Math.log deeply
// enough to find some way of optimizing this code to be small enough to inline.
function bar(a, b) {
    a += b;
    a -= b;
    b ^= a;
    a += Math.log(b);
    b += a;
    b -= a;
    a ^= b;
    a += b;
    a -= b;
    b ^= a;
    a += Math.log(b);
    b += a;
    b -= a;
    a ^= b;
    a += b;
    a -= b;
    b ^= a;
    a += Math.log(b);
    b += a;
    b -= a;
    a ^= b;
    a += b;
    a -= b;
    b ^= a;
    a += Math.log(b);
    b += a;
    b -= a;
    a ^= b;
    a += b;
    a -= b;
    b ^= a;
    a += Math.log(b);
    b += a;
    b -= a;
    a ^= b;
    a += b;
    a -= b;
    b ^= a;
    a += Math.log(b);
    b += a;
    b -= a;
    a ^= b;
    a += b;
    a -= b;
    b ^= a;
    a += Math.log(b);
    b += a;
    b -= a;
    a ^= b;
    a += b;
    a -= b;
    b ^= a;
    a += Math.log(b);
    b += a;
    b -= a;
    a ^= b;
    a += b;
    a -= b;
    b ^= a;
    a += Math.log(b);
    b += a;
    b -= a;
    a ^= b;
    a += b;
    a -= b;
    b ^= a;
    a += Math.log(b);
    b += a;
    b -= a;
    a ^= b;
    a += b;
    a -= b;
    b ^= a;
    a += Math.log(b);
    b += a;
    b -= a;
    a ^= b;
    a += b;
    a -= b;
    b ^= a;
    a += Math.log(b);
    b += a;
    b -= a;
    a ^= b;
    a += b;
    a -= b;
    b ^= a;
    a += Math.log(b);
    b += a;
    b -= a;
    a ^= b;
    a += b;
    a -= b;
    b ^= a;
    a += Math.log(b);
    b += a;
    b -= a;
    a ^= b;
    a += b;
    a -= b;
    b ^= a;
    a += Math.log(b);
    b += a;
    b -= a;
    a ^= b;
    a += b;
    a -= b;
    b ^= a;
    a += Math.log(b);
    b += a;
    b -= a;
    a ^= b;
    return (a - b) | 0;
}

// Function into which we will inline foo but not bar.
function baz(a, b) {
    return new foo(bar(2 * a + 1, b - 1), bar(2 * a, b - 1), a);
}

// Do the test. It's crucial that o.a, o.b, and o.c are checked on each
// loop iteration.
for (var i = 0; i < 1000; ++i) {
    var o = baz(i, i + 1);
    shouldBe("o.a", "bar(2 * i + 1, i)");
    shouldBe("o.b", "bar(2 * i, i)");
    shouldBe("o.c", "i");
}
