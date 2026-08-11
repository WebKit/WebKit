//@ requireOptions("--useExplicitResourceManagement=true")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}, expected: ${String(expected)}`);
}

let shouldThrow = false;

function test(val) {
    using x = {
        [Symbol.dispose]() {}
    };
    if (shouldThrow)
        throw 1;
    return val + 1;
}

noInline(test);

shouldThrow = true;
for (let i = 0; i < 20; i++) {
    try { test(i); } catch {}
}
shouldThrow = false;

for (let i = 0; i < testLoopCount; i++)
    shouldBe(test(i), i + 1);
