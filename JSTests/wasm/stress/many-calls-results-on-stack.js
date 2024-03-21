import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";


let wat = `
(module
    (func $check (import "a" "check") (param i64))

    (func $foo (result ${"i64 ".repeat(20)})
        ${"(i64.const 42) ".repeat(20)}
    )

    (func $bar (result ${"i64 ".repeat(20)})
        ${"(i64.const 0) ".repeat(20)}
    )

    (func (export "test")
        call $foo
        call $bar
        ${"(drop) ".repeat(20)}
        call $check
        ${"(drop) ".repeat(19)}
    )
)
`;

function check(value)
{
    assert.eq(value, 42n);
}

async function test() {
    const instance = await instantiate(wat, { a: {check} }, {reference_types: true});
    const {test} = instance.exports;
    for (let i = 0; i < 1e5; ++i)
        test();
}

assert.asyncTest(test());
