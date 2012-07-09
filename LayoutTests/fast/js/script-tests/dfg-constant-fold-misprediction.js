description(
"This tests that a constant folding on a node that has obviously mispredicted type doesn't send the compiler into an infinite loop."
);

// A function with an argument correctly predicted double.
function foo(x) {
    // Two variables holding constants such that the bytecode generation constant folder
    // will not constant fold the division below, but the DFG constant folder will.
    var a = 1;
    var b = 4000;
    // A division that is going to be predicted integer on the first compilation. The
    // compilation will be triggered from the loop below so the slow case counter of the
    // division will be 1, which is too low for the division to be predicted double.
    // If we constant fold this division, we'll have a constant node that is predicted
    // integer but that contains a double. The subsequent addition to x, which is
    // predicted double, will lead the Fixup phase to inject an Int32ToDouble node on
    // the constant-that-was-a-division; subsequent fases in the fixpoint will constant
    // fold that Int32ToDouble. And hence we will have an infinite loop. The correct fix
    // is to disable constant folding of mispredicted nodes; that allows the normal
    // process of correcting predictions (OSR exit profiling, exiting to profiled code,
    // and recompilation with exponential backoff) to take effect so that the next
    // compilation does not make this same mistake.
    var c = (a / b) + x;
    // A pointless loop to force the first compilation to occur before the division got
    // hot. If this loop was not here then the division would be known to produce doubles
    // on the first compilation.
    var d = 0;
    for (var i = 0; i < 1000; ++i)
        d++;
    return c + d;
}

// Call foo() enough times to make totally sure that we optimize.
for (var i = 0; i < 5; ++i)
    shouldBe("foo(0.5)", "1000.50025");


