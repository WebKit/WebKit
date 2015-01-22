description(
"Tests that an OSR exit between CreateArguments and TearOffArguments does not cause a crash."
);

var foo = function(x) {
    function goo(reenter) {
        var captured = x;
        return x + 5;
    }

    // Loop here to get this function to DFG compile.  Because we have captured vars,
    // there will be a forced OSR exit on entry should we call this function again.
    // We need this OSR exit to occur between the DFG generated code for
    // CreateArguments and TearOffArguments in order for the bug to trigger a crash.
    // See https://webkit.org/b/140743.
    while (!dfgCompiled({f: foo}))
        goo();
    return arguments;
}

foo(1); // First call to get the DFG to compile foo() due to its big loop.
foo(1); // Second call to trigger the OSR exit and verify that we don't crash.
