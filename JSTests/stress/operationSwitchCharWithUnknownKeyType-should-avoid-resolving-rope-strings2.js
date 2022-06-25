//@ if $memoryLimited then skip else runDefault("--useConcurrentJIT=false") end
//@ slow!

function f(o) {
    switch (o) {
        case "t":
        case "s":
        case "u":
    }
}
noInline(f);

for (var i = 0; i < 10000; i++) {
    let o;
    // This test needs to allocate a large rope string, which is slow.
    // The following is tweaked so that we only use this large string once each to
    // exercise the llint, baseline, DFG, and FTL, so that the test doesn't run too slow.
    if (i == 0 || i == 99 || i == 200 || i == 9999)
        o = (-1).toLocaleString().padEnd(2 ** 31 - 1, "a");
    else
        o = (-1).toLocaleString().padEnd(2, "a");
    f(o);
}

