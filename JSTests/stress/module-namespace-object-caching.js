function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

async function fn() {
    // Both should have same structure.
    const valueOfModule = await import('./resources/value-of-module.js');
    const toStringModule = await import('./resources/to-string-module.js');

    // These valueOf / toString should not be cached.
    shouldBe(String(toStringModule), `2020`);
    shouldBe(Number(toStringModule), 2020); // valueOf should not be cached since namespace object is impure for absent.

    shouldBe(Number(valueOfModule), 42); // If the above access accidentally cached the value, this will not return 42.
    shouldBe(String(valueOfModule), `42`);
}
fn().catch($vm.abort);
