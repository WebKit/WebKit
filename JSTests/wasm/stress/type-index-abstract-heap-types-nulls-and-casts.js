import * as assert from "../assert.js";

const binary = read("type-index-abstract-heap-types-nulls-and-casts.wasm", "binary");

function testAbstractNullsAndCasts() {
    const m = new WebAssembly.Instance(new WebAssembly.Module(binary)).exports;

    assert.eq(m.nullFuncref(), null);
    assert.eq(m.nullExternref(), null);
    assert.eq(m.nullAnyref(), null);
    assert.eq(m.nullEqref(), null);
    assert.eq(m.nullI31ref(), null);
    assert.eq(m.nullStructref(), null);
    assert.eq(m.nullArrayref(), null);
    assert.eq(m.nullNone(), null);
    assert.eq(m.nullNofunc(), null);
    assert.eq(m.nullNoextern(), null);

    assert.eq(m.testI31IsEq(42), 1);
    assert.eq(m.testStructIsAny(), 1);
    assert.eq(m.testArrayIsStruct(), 0);
    m.castI31ToAny(99);
    m.castStructToStructref();
    m.castArrayToArrayref();

    assert.throws(
        () => m.castFailAnyToStruct(42),
        WebAssembly.RuntimeError,
        "cast"
    );
}

for (let i = 0; i < wasmTestLoopCount; ++i)
    testAbstractNullsAndCasts();
