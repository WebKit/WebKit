import * as assert from '../assert.js';
{
    assert.throws(() => {
        const g = new WebAssembly.Global({value: "i32", mutable: false});
        g.type.call({});
    }, TypeError, "expected |this| value to be an instance of WebAssembly.Global");

    const i32 = new WebAssembly.Global({value: "i32", mutable: false}).type();
    assert.eq(Object.keys(i32).length, 2);
    assert.eq(i32.value, "i32");
    assert.eq(i32.mutable, false);

    const i32m = new WebAssembly.Global({value: "i32", mutable: true}).type();
    assert.eq(i32m.value, "i32");
    assert.eq(i32m.mutable, true);

    const i64 = new WebAssembly.Global({value: "i64", mutable: true}).type();
    assert.eq(i64.value, "i64");

    const f32 = new WebAssembly.Global({value: "f32", mutable: true}).type();
    assert.eq(f32.value, "f32");

    const f64 = new WebAssembly.Global({value: "f64", mutable: true}).type();
    assert.eq(f64.value, "f64");

    const f64n = new WebAssembly.Global(f64).type();
    assert.eq(f64.value, f64n.value);
    assert.eq(f64.mutable, f64n.mutable);
}