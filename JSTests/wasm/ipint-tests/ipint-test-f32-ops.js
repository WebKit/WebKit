import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (func (export "abs") (param f32) (result f32)
        (local.get 0)
        (f32.abs)
        (return)
    )

    (func (export "neg") (param f32) (result f32)
        (local.get 0)
        (f32.neg)
        (return)
    )

    (func (export "ceil") (param f32) (result f32)
        (local.get 0)
        (f32.ceil)
        (return)
    )

    (func (export "floor") (param f32) (result f32)
        (local.get 0)
        (f32.floor)
        (return)
    )

    (func (export "trunc") (param f32) (result f32)
        (local.get 0)
        (f32.trunc)
        (return)
    )

    (func (export "nearest") (param f32) (result f32)
        (local.get 0)
        (f32.nearest)
        (return)
    )

    (func (export "sqrt") (param f32) (result f32)
        (local.get 0)
        (f32.sqrt)
        (return)
    )

    (func (export "add") (param f32 f32) (result f32)
        (local.get 0)
        (local.get 1)
        (f32.add)
        (return)
    )

    (func (export "sub") (param f32 f32) (result f32)
        (local.get 0)
        (local.get 1)
        (f32.sub)
        (return)
    )

    (func (export "mul") (param f32 f32) (result f32)
        (local.get 0)
        (local.get 1)
        (f32.mul)
        (return)
    )

    (func (export "div") (param f32 f32) (result f32)
        (local.get 0)
        (local.get 1)
        (f32.div)
        (return)
    )

    (func (export "min") (param f32 f32) (result f32)
        (local.get 0)
        (local.get 1)
        (f32.min)
        (return)
    )

    (func (export "max") (param f32 f32) (result f32)
        (local.get 0)
        (local.get 1)
        (f32.max)
        (return)
    )

    (func (export "copysign") (param f32 f32) (result f32)
        (local.get 0)
        (local.get 1)
        (f32.copysign)
        (return)
    )
)
`

function close(a, b) {
    return Math.abs(a - b) < 0.001;
}

let assertClose = (x, y) => assert.truthy(close(x, y))

async function test() {
    const instance = await instantiate(wat, {});
    const { abs, neg, ceil, floor, trunc, nearest, sqrt, add, sub, mul, div, min, max, copysign }= instance.exports
    assertClose(abs(1.5), 1.5)
    assertClose(abs(-1.5), 1.5)

    assertClose(neg(1.5), -1.5)
    assertClose(neg(-1.5), 1.5)

    assertClose(ceil(2.25), 3)
    assertClose(ceil(-3.75), -3)

    assertClose(floor(2.25), 2)
    assertClose(floor(-3.75), -4)

    assertClose(trunc(2.75), 2)
    assertClose(trunc(-2.75), -2)

    assertClose(nearest(2.75), 3)
    assertClose(nearest(-2.75), -3)

    assertClose(sqrt(4), 2)
    assertClose(sqrt(2), 1.414)

    assertClose(add(1, 2), 3)
    assertClose(add(3.5, 4.25), 7.75)

    assertClose(sub(1, 2), -1)
    assertClose(sub(7.27, 2.81), 4.46)

    assertClose(mul(2, 4), 8)
    assertClose(mul(2.5, 1.25), 3.125)

    assertClose(div(1, 2), 0.5)
    assertClose(div(22, 7), 3.143)

    assertClose(min(1, 2), 1)
    assertClose(min(3.5, 4.25), 3.5)

    assertClose(max(1, 2), 2)
    assertClose(max(3.5, 4.25), 4.25)

    assertClose(copysign(1, 2), 1)
    assertClose(copysign(-3.5, 4.25), 3.5)
    assertClose(copysign(1.25, -1.6), -1.25)
    assertClose(copysign(-2.56, -1.28), -2.56)
}

assert.asyncTest(test())
