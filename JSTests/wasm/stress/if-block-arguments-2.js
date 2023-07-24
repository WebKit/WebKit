import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

const magic = 4716278880

let wat = `
(module
    (func (export "test") (param $i i32) (result funcref)
        i64.const ${magic}
        local.get $i

        (if (param i64)
            (then
                drop
                f64.const ${magic}
                drop
                i64.const ${magic}
                drop
            )
            (else
                drop
                i64.const ${magic}
                drop
            )
        )

        ref.null func

        return
    )
)  
`

async function test() {
    const instance = await instantiate(wat, {}, { reference_types: true })
    const { test } = instance.exports

    for (let i = 0; i < 10000; ++i) {
        // assert.eq(test(0), null)
        // assert.eq(test(1)[1], null)

        // try { test(magic)[1]() } catch { }
        // try { test(1)[1].x } catch { }
        try { test(0)() } catch { }
        try { test(1)() } catch { }
    }
}

assert.asyncTest(test())
