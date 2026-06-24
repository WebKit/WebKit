//@ runDefault("--jitPolicyScale=0.1")
/*
(module
    (func (export "foo") (result i32)
        i32.const 42
    )
)
*/

const WASM_CODE = new Uint8Array([0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x05, 0x01, 0x60, 0x00, 0x01, 0x7f, 0x03, 0x02, 0x01, 0x00, 0x07, 0x07, 0x01, 0x03, 0x66, 0x6f, 0x6f, 0x00, 0x00, 0x0a, 0x06, 0x01, 0x04, 0x00, 0x41, 0x2a, 0x0b]);

function bury(f, n) {
    if (n === 0) {
        return f();
    }

    return bury(f, n - 1);
}

function main() {
    const mod = new WebAssembly.Module(WASM_CODE);

    function warmUpInstanceB() {
        const instanceB = new WebAssembly.Instance(mod);

        instanceB.exports.foo();
    }

    bury(warmUpInstanceB, 500);

    const instanceA = new WebAssembly.Instance(mod);

    for (let i = 0; i < 500; i++)
        instanceA.exports.foo();

    gc();

    print("done (should have crashed above)");

}

main();
