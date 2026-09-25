import * as assert from '../assert.js';

// (module
//   (func (export "f") (result i32)
//     ;; body: just `end` -- nothing on stack but result type is i32 -> body validation
//     ;; failure that lazy parsing skips at compile time.
//   )
// )
//
// Hand-encoded so we don't rely on the Builder pre-validating the body.
const invalidBytes = new Uint8Array([
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, // magic + version

    // Type section: () -> i32
    0x01, 0x05, 0x01, 0x60, 0x00, 0x01, 0x7f,

    // Function section: 1 function of type 0
    0x03, 0x02, 0x01, 0x00,

    // Export section: export "f" as func 0
    0x07, 0x05, 0x01, 0x01, 0x66, 0x00, 0x00,

    // Code section: 1 body, body size 2, 0 locals, end (no value pushed)
    0x0a, 0x04, 0x01, 0x02, 0x00, 0x0b,
]);

// (module
//   (func (export "f") (result i32) i32.const 42)
// )
const validBytes = new Uint8Array([
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
    0x01, 0x05, 0x01, 0x60, 0x00, 0x01, 0x7f,
    0x03, 0x02, 0x01, 0x00,
    0x07, 0x05, 0x01, 0x01, 0x66, 0x00, 0x00,
    0x0a, 0x06, 0x01, 0x04, 0x00, 0x41, 0x2a, 0x0b,
]);

// A deferred body-validation failure has to surface as a CompileError, not as whatever else
// calling the export might have gone wrong with.
function assertFirstCallReportsCompileError(instance, description) {
    let caught;
    try {
        instance.exports.f();
    } catch (e) {
        caught = e;
    }
    assert.instanceof(caught, WebAssembly.CompileError, description);
}

// 1. Without the option: lazy validation. Module/Instance construction succeeds;
//    error surfaces only on first call.
{
    const module = new WebAssembly.Module(invalidBytes);
    const instance = new WebAssembly.Instance(module);
    assertFirstCallReportsCompileError(instance, "lazy validation failure on first call");
}

// 2. With { eagerValidate: true } on the Module constructor: throws CompileError at
//    construction time.
assert.throws(
    () => new WebAssembly.Module(invalidBytes, { eagerValidate: true }),
    WebAssembly.CompileError,
    ""
);

// 3. With { eagerValidate: false } the lazy default is preserved.
{
    const module = new WebAssembly.Module(invalidBytes, { eagerValidate: false });
    const instance = new WebAssembly.Instance(module);
    assertFirstCallReportsCompileError(instance, "lazy validation failure on first call (eagerValidate: false)");
}

// 4. Wrong type for the field is a TypeError on options parsing.
assert.throws(
    () => new WebAssembly.Module(invalidBytes, { eagerValidate: "yes" }),
    TypeError,
    "eagerValidate"
);

// 5. WebAssembly.compile WITHOUT the option resolves; failure surfaces on first call.
async function testCompileLazy() {
    const module = await WebAssembly.compile(invalidBytes);
    const instance = new WebAssembly.Instance(module);
    assertFirstCallReportsCompileError(instance, "lazy validation failure on first call (compile path)");
}

// 6. WebAssembly.compile WITH eagerValidate: true rejects with CompileError.
async function testCompileEager() {
    let caught;
    try {
        await WebAssembly.compile(invalidBytes, { eagerValidate: true });
    } catch (e) {
        caught = e;
    }
    assert.instanceof(caught, WebAssembly.CompileError);
}

// 7. WebAssembly.instantiate(bytes, ..., { eagerValidate: true }) rejects with CompileError.
async function testInstantiateEager() {
    let caught;
    try {
        await WebAssembly.instantiate(invalidBytes, {}, { eagerValidate: true });
    } catch (e) {
        caught = e;
    }
    assert.instanceof(caught, WebAssembly.CompileError);
}

// 8. WebAssembly.instantiate(bytes, ...) without the option resolves; first call throws.
async function testInstantiateLazy() {
    const { instance } = await WebAssembly.instantiate(invalidBytes, {});
    assertFirstCallReportsCompileError(instance, "lazy validation failure on first call (instantiate path)");
}

// 9. WebAssembly.validate accepts the option without erroring; result reflects body validity.
assert.truthy(WebAssembly.validate(validBytes, { eagerValidate: true }));
assert.truthy(!WebAssembly.validate(invalidBytes, { eagerValidate: true }));

await assert.asyncTest(testCompileLazy());
await assert.asyncTest(testCompileEager());
await assert.asyncTest(testInstantiateEager());
await assert.asyncTest(testInstantiateLazy());
