import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

let wat = `
(module
    (func (export "test") (param i64) (result i64)
        i64.const 3
        block (param i64) (result i64)
            block (param i64) (result i64)
                local.get 0
                i64.const 2
                i64.ne
                br_table 0 2 2
            end
            drop
            i64.const 4
        end
    )

    (func (export "testAlwaysTrue") (result i64)
        i64.const 3
        block (param i64) (result i64)
            block (param i64) (result i64)
                i64.const 1
                i64.const 2
                i64.ne
                br_table 0 2 2
            end
            drop
            i64.const 4
        end
    )

    (func (export "testAlwaysFalse") (result i64)
        i64.const 3
        block (param i64) (result i64)
            block (param i64) (result i64)
                i64.const 2
                i64.const 2
                i64.ne
                br_table 0 2 2
            end
            drop
            i64.const 4
        end
    )
)
`;

async function test() {
    const instance = await instantiate(wat, {}, {});
    const { test, testAlwaysTrue, testAlwaysFalse } = instance.exports;
    assert.eq(test(1n), 3n);
    assert.eq(test(2n), 4n);
    assert.eq(testAlwaysTrue(), 3n);
    assert.eq(testAlwaysFalse(), 4n);
}

await assert.asyncTest(test());
