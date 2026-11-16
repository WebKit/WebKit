// https://bugs.webkit.org/show_bug.cgi?id=316985
// Streaming instantiate must not treat the result promise as the import object
// when the import argument is omitted.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + " (expected " + expected + ")");
}

// (module
//   (type (func (result i32)))
//   (import "p" "f" (func (type 0)))
//   (func (type 0) (call 0))
//   (export "run" (func 1))
// )
const bytes = new Uint8Array([
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
    0x01, 0x05, 0x01, 0x60, 0x00, 0x01, 0x7f,
    0x02, 0x07, 0x01, 0x01, 0x70, 0x01, 0x66, 0x00, 0x00,
    0x03, 0x02, 0x01, 0x00,
    0x07, 0x07, 0x01, 0x03, 0x72, 0x75, 0x6e, 0x00, 0x01,
    0x0a, 0x06, 0x01, 0x04, 0x00, 0x10, 0x00, 0x0b,
]);

async function shouldReject(promise) {
    let rejected = false;
    try {
        await promise;
    } catch (e) {
        rejected = true;
    }
    shouldBe(rejected, true);
}

async function main() {
    // Pollute Promise.prototype so a wrong importObject (== result promise)
    // would resolve imports via the prototype chain.
    let getterReceiver;
    Object.defineProperty(Promise.prototype, "p", {
        configurable: true,
        get() {
            getterReceiver = this;
            return { f() { return 1337; } };
        },
    });

    try {
        await shouldReject(WebAssembly.instantiate(bytes));

        getterReceiver = undefined;
        const streamingPromise = $vm.createWasmStreamingCompilerForInstantiate(function (compiler) {
            compiler.addBytes(bytes);
        });
        await shouldReject(streamingPromise);
        // Getter must not have been invoked on the result promise.
        shouldBe(getterReceiver, undefined);

        // With a real import object, streaming instantiate still works.
        const { instance } = await $vm.createWasmStreamingCompilerForInstantiate(function (compiler) {
            compiler.addBytes(bytes);
        }, {
            p: { f() { return 42; } },
        });
        shouldBe(instance.exports.run(), 42);
    } finally {
        delete Promise.prototype.p;
    }
}

main().catch(function (error) {
    print(String(error));
    print(String(error.stack));
    $vm.abort();
});
