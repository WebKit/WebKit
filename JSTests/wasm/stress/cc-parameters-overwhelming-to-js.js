import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (type $sig_test (func (param i32) (result i32)))

    (import "o" "callee" (func $callee
        (param $a i32) (param $b i32) (param $c i32) (param $d i32) (param $e i32) (param $f i32) (param $g i32) (param $h i32) (param $i i32) (param $j i32) (param $k i32) (param $l i32) (param $m i32) (param $n i32) (param $o i32) (param $p i32)
        (result i32))
    )

    (func $test (export "test") (param $x i32) (result i32)
        (i32.const 0)
        (i32.const 1)
        (i32.const 2)
        (i32.const 3)
        (i32.const 4)
        (i32.const 5)
        (i32.const 6)
        (i32.const 7)
        (i32.const 8)
        (i32.const 9)
        (i32.const 10)
        (i32.const 11)
        (i32.const 12)
        (i32.const 13)
        (i32.const 14)
        (i32.const 15)
        (call $callee)
    )
)
`

function callee(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p) {
    assert.eq(a, 0);
    assert.eq(b, 1);
    assert.eq(c, 2);
    assert.eq(d, 3);
    assert.eq(e, 4);
    assert.eq(f, 5);
    assert.eq(g, 6);
    assert.eq(h, 7);
    assert.eq(i, 8);
    assert.eq(j, 9);
    assert.eq(k, 10);
    assert.eq(l, 11);
    assert.eq(m, 12);
    assert.eq(n, 13);
    assert.eq(o, 14);
    assert.eq(p, 15);
    return 42;
}

async function test() {
    const instance = await instantiate(wat, { o: { callee } })
    const { test } = instance.exports

    assert.eq(test(5), 42)
}

await assert.asyncTest(test())
