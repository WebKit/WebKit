// Regression test: Wasm::Table::grow on a funcref table with a non-null
// default left slots in an inconsistent state where m_value held a live
// WebAssemblyFunction wrapper but m_function.rtt was null. Because
// Function::isEmpty() == !m_function.rtt and visitAggregateImpl skipped
// visiting m_value on "empty" slots, the wrapper went unmarked, GC freed
// it, and table.get returned a dangling pointer.

if (!this.WebAssembly)
    quit(0);

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + " (expected " + expected + ")");
}

// (module (func $f (export "f") (result i32) (i32.const <constant>)))
// Each slot gets its own module/instance/function so identity is unambiguous.
function makeModule(constant) {
    const leb = [];
    let v = constant;
    while (true) {
        const b = v & 0x7f;
        v >>>= 7;
        if (v === 0 && (b & 0x40) === 0) {
            leb.push(b);
            break;
        }
        leb.push(b | 0x80);
    }
    const bodyLen = 3 + leb.length;
    const codeLen = 2 + bodyLen;
    return new WebAssembly.Module(new Uint8Array([
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
        0x01, 0x05, 0x01, 0x60, 0x00, 0x01, 0x7f,
        0x03, 0x02, 0x01, 0x00,
        0x07, 0x05, 0x01, 0x01, 0x66, 0x00, 0x00,
        0x0a, codeLen, 0x01, bodyLen, 0x00, 0x41, ...leb, 0x0b,
    ]));
}

const N = 1000;
const table = new WebAssembly.Table({ element: "funcref", initial: 0 });

// Each slot's only strong reference is the m_value WriteBarrier on the slot
// itself; the `fn` local goes dead between iterations.
for (let i = 0; i < N; ++i) {
    const fn = new WebAssembly.Instance(makeModule(i)).exports.f;
    table.grow(1, fn);
}

fullGC();

for (let i = 0; i < N; ++i) {
    const stale = table.get(i);
    if (typeof stale !== "function")
        throw new Error("table.get(" + i + ") was not a function: " + stale);
    shouldBe(stale(), i);
}
