// Regression test: new WebAssembly.Table({element: "funcref", initial: N}, value)
// with a WebAssemblyWrapperFunction default value (a JS function imported into
// wasm and re-exported) passed the constructor's type check but the fill loop
// only stored WebAssemblyFunction defaults, leaving every funcref entry null.
// Table.prototype.set and Table.prototype.grow stored the same value correctly.

if (!this.WebAssembly)
    quit(0);

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + " (expected " + expected + ")");
}

// (module (import "m" "f" (func (result i32))) (export "f" (func 0)))
const reexportingModule = new WebAssembly.Module(new Uint8Array([
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
    0x01, 0x05, 0x01, 0x60, 0x00, 0x01, 0x7f,
    0x02, 0x07, 0x01, 0x01, 0x6d, 0x01, 0x66, 0x00, 0x00,
    0x07, 0x05, 0x01, 0x01, 0x66, 0x00, 0x00,
]));

// (module (func (export "f") (result i32) (i32.const 7)))
const exportingModule = new WebAssembly.Module(new Uint8Array([
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
    0x01, 0x05, 0x01, 0x60, 0x00, 0x01, 0x7f,
    0x03, 0x02, 0x01, 0x00,
    0x07, 0x05, 0x01, 0x01, 0x66, 0x00, 0x00,
    0x0a, 0x06, 0x01, 0x04, 0x00, 0x41, 0x07, 0x0b,
]));

// (module (import "m" "t" (table 3 funcref))
//         (func (export "call") (param i32) (result i32)
//             (call_indirect (result i32) (local.get 0))))
const callingModule = new WebAssembly.Module(new Uint8Array([
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
    0x01, 0x0a, 0x02, 0x60, 0x00, 0x01, 0x7f, 0x60, 0x01, 0x7f, 0x01, 0x7f,
    0x02, 0x09, 0x01, 0x01, 0x6d, 0x01, 0x74, 0x01, 0x70, 0x00, 0x03,
    0x03, 0x02, 0x01, 0x01,
    0x07, 0x08, 0x01, 0x04, 0x63, 0x61, 0x6c, 0x6c, 0x00, 0x00,
    0x0a, 0x09, 0x01, 0x07, 0x00, 0x20, 0x00, 0x11, 0x00, 0x00, 0x0b,
]));

const wrapper = new WebAssembly.Instance(reexportingModule, { m: { f: () => 42 } }).exports.f;

for (const element of ["funcref", "anyfunc"]) {
    const table = new WebAssembly.Table({ element, initial: 3 }, wrapper);
    for (let i = 0; i < 3; ++i) {
        shouldBe(table.get(i), wrapper);
        shouldBe(table.get(i)(), 42);
    }

    // The wasm-callable half of each slot must be populated too: call_indirect
    // through the constructor-filled slots dispatches to the wrapped JS function.
    const caller = new WebAssembly.Instance(callingModule, { m: { t: table } }).exports.call;
    for (let i = 0; i < 3; ++i)
        shouldBe(caller(i), 42);
}

// Constructor-filled slots keep their values alive across GC.
{
    let table;
    (function () {
        const transientWrapper = new WebAssembly.Instance(reexportingModule, { m: { f: () => 13 } }).exports.f;
        table = new WebAssembly.Table({ element: "funcref", initial: 2 }, transientWrapper);
    })();
    if (typeof fullGC === "function")
        fullGC();
    for (let i = 0; i < 2; ++i) {
        shouldBe(typeof table.get(i), "function");
        shouldBe(table.get(i)(), 13);
    }
}

// WebAssemblyFunction default values keep working.
const wasmFunction = new WebAssembly.Instance(exportingModule).exports.f;
const table = new WebAssembly.Table({ element: "funcref", initial: 2 }, wasmFunction);
for (let i = 0; i < 2; ++i) {
    shouldBe(table.get(i), wasmFunction);
    shouldBe(table.get(i)(), 7);
}

// Null and absent default values keep producing null entries.
shouldBe(new WebAssembly.Table({ element: "funcref", initial: 1 }, null).get(0), null);
shouldBe(new WebAssembly.Table({ element: "funcref", initial: 1 }).get(0), null);

// Non-function default values keep throwing.
let threw = false;
try {
    new WebAssembly.Table({ element: "funcref", initial: 1 }, () => 1);
} catch (error) {
    threw = true;
    shouldBe(error instanceof TypeError, true);
}
shouldBe(threw, true);

// Externref tables keep storing arbitrary default values.
const externTable = new WebAssembly.Table({ element: "externref", initial: 2 }, "hello");
shouldBe(externTable.get(0), "hello");
shouldBe(externTable.get(1), "hello");
