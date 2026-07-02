//@ runDefault("--useConcurrentJIT=false", "--sweepSynchronously=true")

// This test passes if it does not crash with an ASAN build.

(function() {
    var bar = {};

    for (var i = 0; i < 68; ++i)
        String.raw`boo`;

    eval(String.raw`bar += 0;`);
    eval(String.raw`bar += 0;`);
    eval(String.raw`bar += 0;`);
    eval(String.raw`bar += 0;`);
    eval(String.raw`bar += 0;`);
    eval(String.raw`bar += 0;`);
    eval(String.raw`bar += 0;`);
    eval(String.raw`bar += 0;`);
    eval(String.raw`bar += 0;`);
    eval(String.raw`bar += 0;`);

    eval(String.raw`bar += 0;`);
    eval(String.raw`bar += 0;`);
    eval(String.raw`bar += 0;`);
    eval(String.raw`bar += 0;`);
    eval(String.raw`bar += 0;`);
    eval(String.raw`bar += 0;`);
    eval(String.raw`bar += 0;`);
    eval(String.raw`bar += 0;`);
    eval(String.raw`bar += 0;`);
    eval(String.raw`bar += 0;`);

    eval(String.raw`bar += 0;`);
    eval(String.raw`bar += 0;`);
    eval(String.raw`bar += 0;`);
    eval(String.raw`bar += 0;`);
    eval(String.raw`bar += 0;`);
    eval(String.raw`bar += 0;`);
    eval(String.raw`bar += 0;`);
    eval(String.raw`bar += 0;`);
    eval(String.raw`bar += 0;`);
    eval(String.raw`bar += 0;`);

    eval(String.raw`bar += 0;`);
    eval(String.raw`bar += 0;`);
    eval(String.raw`bar += 0;`);
    eval(String.raw`bar += 0;`);
    eval(String.raw`bar += 0;`);
    eval(String.raw`bar += 0;`);
    eval(String.raw`bar += 0;`);
    eval(String.raw`bar += 0;`);
    eval(String.raw`bar += 0;`);
    eval(String.raw`bar += 0;`);

    eval(String.raw`bar += 0;`);
    eval(String.raw`bar += 0;`);
    eval(String.raw`bar += 0;`);
    eval(String.raw`bar += 0;`);
    eval(String.raw`bar += 0;`);
    eval(String.raw`bar += 0;`);
    eval(String.raw`bar += 0;`);
    eval(String.raw`bar += 0;`);
    eval(String.raw`bar += 0;`);
    eval(String.raw`bar += 0;`);

    eval(String.raw`bar += 0;`);
    eval(String.raw`bar += 0;`);
    eval(String.raw`bar += 0;`);
    eval(String.raw`bar += 0;`);
    eval(String.raw`bar += 0;`);
    eval(String.raw`bar += 0;`);
    eval(String.raw`bar += 0;`);
    eval(String.raw`bar += 0;`);
    eval(String.raw`bar += 0;`);
    eval(String.raw`bar += 0;`);

    eval(String.raw`bar += 0;`);
    eval(String.raw`bar += 0;`);
    eval(String.raw`bar += 0;`);
    eval(String.raw`bar += 0;`);
    eval(String.raw`bar += 0;`);

    eval(String.raw`foo = class { };`);
    foo += 0;

    gc();
    try {
        eval(foo.toString());
    } catch (e) {
        exception = e;
    }

    if (exception != "SyntaxError: Class statements must have a name.")
        throw "FAIL";
})();
