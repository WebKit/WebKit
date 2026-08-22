//@ requireOptions("--useConcurrentJIT=0")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Expected ${expected} but got ${actual}`);
}

const state = { throwOnDispose: false };

const resources = [];
for (let i = 0; i < 16; ++i) {
    resources.push({
        state,
        [Symbol.dispose]: new Function(`if (this.state.throwOnDispose) throw new Error("dispose ${i}");`),
    });
}

function run(resource, bodyShouldThrow) {
    try {
        {
            using r = resource;
            if (bodyShouldThrow)
                throw new Error("body");
        }
    } catch (e) {
        return e;
    }
    return null;
}

for (let i = 0; i < testLoopCount; ++i) {
    const error = run(resources[i % resources.length], i & 1);
    shouldBe(error === null, !(i & 1));
}

state.throwOnDispose = true;
const error = run(resources[0], 1);
shouldBe(error instanceof SuppressedError, true);
shouldBe(error.error.message, "dispose 0");
shouldBe(error.suppressed.message, "body");
