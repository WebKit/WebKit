//@ requireOptions("--thresholdForOMGOptimizeAfterWarmUp=0")
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

let wat = `
(module
    (type $0 (func (param funcref)))
    (import "jsModule" "foo" (func $1 (type $0)))
    (func (export "bar") (local i32)
        ref.func $1
        loop (param funcref)
            try (param funcref)
                call 0
            end
        end
    )
    (elem declare func $1)
)
`

function foo(f) {
    f?.x;
}

async function test() {
    let jsModule = { foo };
    const instance = await instantiate(wat, { jsModule }, { exceptions: true });
    const { bar } = instance.exports;
    for (let i = 0; i < 10000; i ++)
        bar();
}

await assert.asyncTest(test())
