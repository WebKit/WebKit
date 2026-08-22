//@ requireOptions("--useConcurrentJIT=0", "--validateGraph=1")

function test() {
    const resource = { [Symbol.dispose]() { } };
    for (using r of [resource]) {
        try { r(); } catch (e) { }
    }
}

for (let i = 0; i < testLoopCount; ++i)
    test();
