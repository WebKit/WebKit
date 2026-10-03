//@ slow!
// https://bugs.webkit.org/show_bug.cgi?id=247454
import * as assert from "../assert.js";

const binary = read("type-index-abstract-heap-types-globals-and-tables.wasm", "binary");

function testGlobalsAndTables() {
    const m = new WebAssembly.Instance(new WebAssembly.Module(binary)).exports;

    assert.eq(m.gf.value, null);
    m.setGlobal(null);
    assert.eq(m.getGlobal(), null);
    m.setTable(1, "hello");
    assert.eq(m.getTable(1), "hello");
    assert.eq(m.nullInTable(), 1);
}

for (let i = 0; i < wasmTestLoopCount; ++i)
    testGlobalsAndTables();
