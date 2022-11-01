function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

const log = globalThis.console?.log ?? debug;

let expected = "--> CompileError: WebAssembly.Module doesn't validate: attempt to use unknown local 5, the number of locals is 5, in function at index 0";

/*
(module
  (type (;0;) (func (result i64)))
  (func (;0;) (type 0) (result i64)
    (local i64 i64 i64 i64 i64)
    local.get 5))
*/

(async function () {
    let bytes = new Uint8Array([0x0, 0x61, 0x73, 0x6d, 0x1, 0x0, 0x0, 0x0, 0x1, 0x5, 0x1, 0x60, 0x0, 0x1, 0x7e, 0x3, 0x2, 0x1, 0x0, 0xa, 0x8, 0x1, 0x6, 0x1, 0x5, 0x7e, 0x20, 0x5, 0xb]);
    await WebAssembly.instantiate(bytes, {});
})()
    .then(() => log('bye'))
    .catch(e => shouldBe(e, expected));
