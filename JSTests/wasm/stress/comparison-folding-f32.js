import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

let wat = `
(module
    (func (export "testEqTrue") (result i32)
        f32.const 42
        f32.const 42
        f32.eq
    )

    (func (export "testEqFalse") (result i32)
        f32.const 41
        f32.const 42
        f32.eq
    )

    (func (export "testNeTrue") (result i32)
        f32.const 41
        f32.const 42
        f32.ne
    )

    (func (export "testNeFalse") (result i32)
        f32.const 42
        f32.const 42
        f32.ne
    )

    (func (export "testLtTrue") (result i32)
        f32.const -8
        f32.const 17
        f32.lt
    )

    (func (export "testLtFalse") (result i32)
        f32.const 17
        f32.const 9
        f32.lt
    )

    (func (export "testLeTrue") (result i32)
        f32.const 19
        f32.const 19
        f32.le
    )

    (func (export "testLeFalse") (result i32)
        f32.const 17
        f32.const 9
        f32.le
    )

    (func (export "testGtTrue") (result i32)
        f32.const 17
        f32.const -8
        f32.gt
    )

    (func (export "testGtFalse") (result i32)
        f32.const 9
        f32.const 17
        f32.gt
    )

    (func (export "testGeTrue") (result i32)
        f32.const 19
        f32.const 19
        f32.ge
    )

    (func (export "testGeFalse") (result i32)
        f32.const 9
        f32.const 17
        f32.ge
    )
)
`;

async function test() {
    const instance = await instantiate(wat, {}, {});
    const {
        testEqTrue, testEqFalse, 
        testNeTrue, testNeFalse,
        testLtTrue, testLtFalse,
        testLeTrue, testLeFalse,
        testGtTrue, testGtFalse,
        testGeTrue, testGeFalse,
    } = instance.exports;
    assert.eq(testEqTrue(), 1);
    assert.eq(testEqFalse(), 0);
    assert.eq(testNeTrue(), 1);
    assert.eq(testNeFalse(), 0);
    assert.eq(testLtTrue(), 1);
    assert.eq(testLtFalse(), 0);
    assert.eq(testLeTrue(), 1);
    assert.eq(testLeFalse(), 0);
    assert.eq(testGtTrue(), 1);
    assert.eq(testGtFalse(), 0);
    assert.eq(testGeTrue(), 1);
    assert.eq(testGeFalse(), 0);
}

await assert.asyncTest(test());
