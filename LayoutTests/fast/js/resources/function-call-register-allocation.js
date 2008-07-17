description(
"This test checks for a specific regression that caused function calls to allocate too many temporary registers."
);

var message = "PASS: Recursion did not run out of stack space."
try {
    // Call a function recursively.
    (function f(g, x) {
        if (x > 3000)
            return;

        // Do lots of function calls -- when the bug was present, each
        // of these calls would allocate a new temporary register. We can
        // detect profligate register allocation because it will substantially
        // curtail our recursion limit.
        g(); g(); g(); g(); g(); g(); g(); g(); g(); g();
        g(); g(); g(); g(); g(); g(); g(); g(); g(); g();
        g(); g(); g(); g(); g(); g(); g(); g(); g(); g();
        g(); g(); g(); g(); g(); g(); g(); g(); g(); g();
        g(); g(); g(); g(); g(); g(); g(); g(); g(); g();
        g(); g(); g(); g(); g(); g(); g(); g(); g(); g();
        g(); g(); g(); g(); g(); g(); g(); g(); g(); g();
        g(); g(); g(); g(); g(); g(); g(); g(); g(); g();
        g(); g(); g(); g(); g(); g(); g(); g(); g(); g();
        g(); g(); g(); g(); g(); g(); g(); g(); g(); g();
        g(); g(); g(); g(); g(); g(); g(); g(); g(); g();
        g(); g(); g(); g(); g(); g(); g(); g(); g(); g();
        g(); g(); g(); g(); g(); g(); g(); g(); g(); g();
        g(); g(); g(); g(); g(); g(); g(); g(); g(); g();
        g(); g(); g(); g(); g(); g(); g(); g(); g(); g();
        g(); g(); g(); g(); g(); g(); g(); g(); g(); g();

        f(g, ++x);
    })(function() {}, 0);
} catch(e) {
    message = "FAIL: Recursion threw an exception: " + e;
}

debug(message);

var successfullyParsed = true;
