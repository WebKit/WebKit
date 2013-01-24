description(
"Tests that constant folding an access to an uncaptured variable that is captured later in the same basic block doesn't lead to assertion failures."
);

var thingy = 456;

function bar() {
    return thingy;
}

function baz(a) {
    if (a) // Here we have an access to r2. The bug was concerned with our assertions thinking that this access was invalid.
        return arguments; // Force r2 (see below) to get captured.
}

function foo(p, a) {
    // The temporary variable corresponding to the 'bar' callee coming out of the ternary expression will be allocated by
    // the bytecompiler to some virtual register, say r2. This expression is engineered so that (1) the virtual register
    // chosen for the callee here is the same as the one that will be chosen for the first non-this argument below,
    // (2) that the callee ends up being constant but requires CFA to prove it, and (3) that we actually load that constant
    // using GetLocal (which happens because of the CheckFunction to check the callee).
    var x = (a + 1) + (p ? bar : bar)();
    // The temporary variable corresponding to the first non-this argument to baz will be allocated to the same virtual
    // register (i.e. r2).
    return baz(x);
}

for (var i = 0; i < 100; ++i)
    shouldBe("foo(true, 5)[0]", "462");
