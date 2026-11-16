import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (func (export "f32_min") (param f32 f32) (result f32)
        local.get 0
        local.get 1
        f32.min
    )
    (func (export "f32_max") (param f32 f32) (result f32)
        local.get 0
        local.get 1
        f32.max
    )
    (func (export "f64_min") (param f64 f64) (result f64)
        local.get 0
        local.get 1
        f64.min
    )
    (func (export "f64_max") (param f64 f64) (result f64)
        local.get 0
        local.get 1
        f64.max
    )
)
`

async function test() {
    const instance = await instantiate(wat, {}, {});
    const { f32_min, f32_max, f64_min, f64_max } = instance.exports;

    let f32_subnormal = Math.pow(2, -126) * (1 - Math.pow(2, -23)); 
    for (let comparand of [0.0, 1234.1, f32_subnormal, Infinity]) {
        assert.eq(f32_min(comparand, NaN), NaN);
        assert.eq(f32_min(NaN, comparand), NaN);
        assert.eq(f32_min(-comparand, NaN), NaN);
        assert.eq(f32_min(NaN, -comparand), NaN);

        assert.eq(f32_max(comparand, NaN), NaN);
        assert.eq(f32_max(NaN, comparand), NaN);
        assert.eq(f32_max(-comparand, NaN), NaN);
        assert.eq(f32_max(NaN, -comparand), NaN);
    }

    let f64_subnormal = Math.pow(2, -1022) * (1 - Math.pow(2, -52));
    for (let comparand of [0.0, 1234.1, f64_subnormal, Infinity]) {
        assert.eq(f64_min(comparand, NaN), NaN);
        assert.eq(f64_min(NaN, comparand), NaN);
        assert.eq(f64_min(-comparand, NaN), NaN);
        assert.eq(f64_min(NaN, -comparand), NaN);

        assert.eq(f64_max(comparand, NaN), NaN);
        assert.eq(f64_max(NaN, comparand), NaN);
        assert.eq(f64_max(-comparand, NaN), NaN);
        assert.eq(f64_max(NaN, -comparand), NaN);
    }
}

await assert.asyncTest(test());
