//@ slow!
// https://bugs.webkit.org/show_bug.cgi?id=247454
import * as assert from "../assert.js";

const binary = read("type-index-abstract-heap-types-concrete-vs-abstract.wasm", "binary");

function testConcreteVsAbstract() {
    const m = new WebAssembly.Instance(new WebAssembly.Module(binary)).exports;

    const v = m.asAny();
    assert.eq(m.testIsS(v), 1);
    m.castToS(v);
    m.castToS(m.asStruct());
    assert.eq(m.testIsS(null), 1);
    assert.eq(m.testIsS(42), 0);
}

for (let i = 0; i < wasmTestLoopCount; ++i)
    testConcreteVsAbstract();
