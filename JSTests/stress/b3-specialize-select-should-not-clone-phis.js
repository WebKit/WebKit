//@ runDefault("--useConcurrentJIT=0", "--forceEagerCompilation=1")

// B3's specializeSelect phase clones every value between a Select and the Check it feeds into
// both arms of the branch it introduces, rewiring the clones by remapping their children. A Phi
// cannot be rewired that way: its value comes from an Upsilon writing the Phi's local-state slot,
// so the clone was an orphan Phi holding garbage, and an Upsilon then forwarded that garbage into
// the original Phi. Here the Phi is the loop variable, so `v` became a bogus JSValue that
// operationValueAddProfiled then dereferenced as a cell.
function f(g) {
    const b = !g;
    let v = b;
    for (; v < 100; v = v + 100)
        b.x = 1;
    return v;
}

for (let i = 0; i < 2000; i++) {
    const result = f(f);
    if (result !== 100)
        throw new Error(`bad result at iteration ${i}: ${result}`);
}
