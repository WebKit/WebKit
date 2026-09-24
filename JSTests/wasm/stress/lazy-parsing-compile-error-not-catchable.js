import * as assert from "../assert.js";

// A function body is validated on the first call into it, which puts the resulting
// WebAssembly.CompileError on a frame inside the wasm call stack. No wasm handler may observe
// it: a module that does not validate is not a wasm-level throw, and a caller wrapping the call
// in catch_all must not be able to turn the failure into a normal return.

function leb(value) {
    let bytes = [];
    do {
        let byte = value & 0x7f;
        value >>>= 7;
        if (value)
            byte |= 0x80;
        bytes.push(byte);
    } while (value);
    return bytes;
}

function section(id, payload) {
    return [id, ...leb(payload.length), ...payload];
}

function body(code) {
    const withLocals = [0x00, ...code];
    return [...leb(withLocals.length), ...withLocals];
}

// Two functions of type () -> i32: function 0 has a body that returns nothing, function 1 is
// exported as "f" and calls function 0 under the handler given by `handlerBody`.
function moduleBytes(handlerBody) {
    let bytes = [0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00];
    bytes.push(...section(1, [0x01, 0x60, 0x00, 0x01, 0x7f]));
    bytes.push(...section(3, [0x02, 0x00, 0x00]));
    bytes.push(...section(7, [0x01, 0x01, 0x66, 0x00, 0x01]));
    bytes.push(...section(10, [0x02, ...body([0x0b]), ...body(handlerBody)]));
    return new Uint8Array(bytes);
}

// (try (result i32) (do (call 0)) (catch_all (i32.const 42)))
const legacyCatchAll = [
    0x06, 0x7f,       // try (result i32)
    0x10, 0x00,       // call 0
    0x19,             // catch_all
    0x41, 0x2a,       // i32.const 42
    0x0b,             // end try
    0x0b,             // end function
];

// (block $handler (try_table (result i32) (catch_all $handler) (call 0)) return) (i32.const 42)
const tryTableCatchAll = [
    0x02, 0x40,             // block
    0x1f, 0x7f, 0x01,       // try_table (result i32), 1 catch clause
    0x02, 0x00,             //   catch_all -> $handler
    0x10, 0x00,             // call 0
    0x0b,                   // end try_table
    0x0f,                   // return
    0x0b,                   // end block
    0x41, 0x2a,             // i32.const 42
    0x0b,                   // end function
];

function assertCallReportsCompileError(description, handlerBody) {
    const bytes = moduleBytes(handlerBody);

    // The handler itself has to be well formed, or the test would be asserting on the wrong
    // failure. Only function 0 is invalid.
    assert.falsy(WebAssembly.validate(bytes));

    const instance = new WebAssembly.Instance(new WebAssembly.Module(bytes));
    // Twice: the second call is served from the cached diagnostic rather than a fresh parse.
    for (let i = 0; i < 2; ++i) {
        let caught;
        try {
            instance.exports.f();
        } catch (e) {
            caught = e;
        }
        assert.instanceof(caught, WebAssembly.CompileError, description);
    }
}

assertCallReportsCompileError("legacy catch_all", legacyCatchAll);
assertCallReportsCompileError("try_table catch_all", tryTableCatchAll);
