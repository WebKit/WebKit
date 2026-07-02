import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

let wat = `
(module
    (func (export "testI32RemU_2") (param i32) (result i32)
        local.get 0
        i32.const 2
        i32.rem_u
    )

    (func (export "testI32RemS_2") (param i32) (result i32)
        local.get 0
        i32.const 2
        i32.rem_s
    )

    (func (export "testI32RemU_32") (param i32) (result i32)
        local.get 0
        i32.const 32
        i32.rem_u
    )

    (func (export "testI32RemS_32") (param i32) (result i32)
        local.get 0
        i32.const 32
        i32.rem_s
    )

    (func (export "testI32RemU_512") (param i32) (result i32)
        local.get 0
        i32.const 512
        i32.rem_u
    )

    (func (export "testI32RemS_512") (param i32) (result i32)
        local.get 0
        i32.const 512
        i32.rem_s
    )

    (func (export "testI32RemU_131072") (param i32) (result i32)
        local.get 0
        i32.const 131072
        i32.rem_u
    )

    (func (export "testI32RemS_131072") (param i32) (result i32)
        local.get 0
        i32.const 131072
        i32.rem_s
    )

    (func (export "testI32RemU_134217728") (param i32) (result i32)
        local.get 0
        i32.const 134217728
        i32.rem_u
    )

    (func (export "testI32RemS_134217728") (param i32) (result i32)
        local.get 0
        i32.const 134217728
        i32.rem_s
    )

    (func (export "testI64RemU_2") (param i64) (result i64)
        local.get 0
        i64.const 2
        i64.rem_u
    )

    (func (export "testI64RemS_2") (param i64) (result i64)
        local.get 0
        i64.const 2
        i64.rem_s
    )

    (func (export "testI64RemU_32") (param i64) (result i64)
        local.get 0
        i64.const 32
        i64.rem_u
    )

    (func (export "testI64RemS_32") (param i64) (result i64)
        local.get 0
        i64.const 32
        i64.rem_s
    )

    (func (export "testI64RemU_512") (param i64) (result i64)
        local.get 0
        i64.const 512
        i64.rem_u
    )

    (func (export "testI64RemS_512") (param i64) (result i64)
        local.get 0
        i64.const 512
        i64.rem_s
    )

    (func (export "testI64RemU_131072") (param i64) (result i64)
        local.get 0
        i64.const 131072
        i64.rem_u
    )

    (func (export "testI64RemS_131072") (param i64) (result i64)
        local.get 0
        i64.const 131072
        i64.rem_s
    )

    (func (export "testI64RemU_134217728") (param i64) (result i64)
        local.get 0
        i64.const 134217728
        i64.rem_u
    )

    (func (export "testI64RemS_134217728") (param i64) (result i64)
        local.get 0
        i64.const 134217728
        i64.rem_s
    )

    (func (export "testI64RemU_8796093022208") (param i64) (result i64)
        local.get 0
        i64.const 8796093022208
        i64.rem_u
    )

    (func (export "testI64RemS_8796093022208") (param i64) (result i64)
        local.get 0
        i64.const 8796093022208
        i64.rem_s
    )
)
`;

async function test() {
    const instance = await instantiate(wat, {}, {});
    const { 
        testI32RemU_2,
        testI32RemS_2,
        testI32RemU_32,
        testI32RemS_32,
        testI32RemU_512,
        testI32RemS_512,
        testI32RemU_131072,
        testI32RemS_131072,
        testI32RemU_134217728,
        testI32RemS_134217728,
        testI64RemU_2,
        testI64RemS_2,
        testI64RemU_32,
        testI64RemS_32,
        testI64RemU_512,
        testI64RemS_512,
        testI64RemU_131072,
        testI64RemS_131072,
        testI64RemU_134217728,
        testI64RemS_134217728,
        testI64RemU_8796093022208,
        testI64RemS_8796093022208
    } = instance.exports;

    assert.eq(testI32RemU_2(0), 0);
    assert.eq(testI32RemU_2(1), 1);
    assert.eq(testI32RemU_2(2), 0);
    assert.eq(testI32RemU_2(7), 1);
    assert.eq(testI32RemU_2(8), 0);
    assert.eq(testI32RemU_2(9), 1);

    assert.eq(testI32RemS_2(0), 0);
    assert.eq(testI32RemS_2(1), 1);
    assert.eq(testI32RemS_2(2), 0);
    assert.eq(testI32RemS_2(7), 1);
    assert.eq(testI32RemS_2(8), 0);
    assert.eq(testI32RemS_2(9), 1);
    assert.eq(testI32RemS_2(-1), -1);
    assert.eq(testI32RemS_2(-2), 0);
    assert.eq(testI32RemS_2(-7), -1);
    assert.eq(testI32RemS_2(-8), 0);
    assert.eq(testI32RemS_2(-9), -1);

    assert.eq(testI64RemU_2(0n), 0n);
    assert.eq(testI64RemU_2(1n), 1n);
    assert.eq(testI64RemU_2(2n), 0n);
    assert.eq(testI64RemU_2(7n), 1n);
    assert.eq(testI64RemU_2(8n), 0n);
    assert.eq(testI64RemU_2(9n), 1n);

    assert.eq(testI64RemS_2(0n), 0n);
    assert.eq(testI64RemS_2(1n), 1n);
    assert.eq(testI64RemS_2(2n), 0n);
    assert.eq(testI64RemS_2(7n), 1n);
    assert.eq(testI64RemS_2(8n), 0n);
    assert.eq(testI64RemS_2(9n), 1n);
    assert.eq(testI64RemS_2(-1n), -1n);
    assert.eq(testI64RemS_2(-2n), 0n);
    assert.eq(testI64RemS_2(-7n), -1n);
    assert.eq(testI64RemS_2(-8n), 0n);
    assert.eq(testI64RemS_2(-9n), -1n);

    assert.eq(testI32RemU_32(0), 0);
    assert.eq(testI32RemU_32(1), 1);
    assert.eq(testI32RemU_32(32), 0);
    assert.eq(testI32RemU_32(127), 31);
    assert.eq(testI32RemU_32(128), 0);
    assert.eq(testI32RemU_32(129), 1);

    assert.eq(testI32RemS_32(0), 0);
    assert.eq(testI32RemS_32(1), 1);
    assert.eq(testI32RemS_32(32), 0);
    assert.eq(testI32RemS_32(127), 31);
    assert.eq(testI32RemS_32(128), 0);
    assert.eq(testI32RemS_32(129), 1);
    assert.eq(testI32RemS_32(-1), -1);
    assert.eq(testI32RemS_32(-32), 0);
    assert.eq(testI32RemS_32(-127), -31);
    assert.eq(testI32RemS_32(-128), 0);
    assert.eq(testI32RemS_32(-129), -1);

    assert.eq(testI64RemU_32(0n), 0n);
    assert.eq(testI64RemU_32(1n), 1n);
    assert.eq(testI64RemU_32(32n), 0n);
    assert.eq(testI64RemU_32(127n), 31n);
    assert.eq(testI64RemU_32(128n), 0n);
    assert.eq(testI64RemU_32(129n), 1n);

    assert.eq(testI64RemS_32(0n), 0n);
    assert.eq(testI64RemS_32(1n), 1n);
    assert.eq(testI64RemS_32(32n), 0n);
    assert.eq(testI64RemS_32(127n), 31n);
    assert.eq(testI64RemS_32(128n), 0n);
    assert.eq(testI64RemS_32(129n), 1n);
    assert.eq(testI64RemS_32(-1n), -1n);
    assert.eq(testI64RemS_32(-32n), 0n);
    assert.eq(testI64RemS_32(-127n), -31n);
    assert.eq(testI64RemS_32(-128n), 0n);
    assert.eq(testI64RemS_32(-129n), -1n);

    assert.eq(testI32RemU_512(0), 0);
    assert.eq(testI32RemU_512(1), 1);
    assert.eq(testI32RemU_512(512), 0);
    assert.eq(testI32RemU_512(4095), 511);
    assert.eq(testI32RemU_512(4096), 0);
    assert.eq(testI32RemU_512(4097), 1);

    assert.eq(testI32RemS_512(0), 0);
    assert.eq(testI32RemS_512(1), 1);
    assert.eq(testI32RemS_512(512), 0);
    assert.eq(testI32RemS_512(4095), 511);
    assert.eq(testI32RemS_512(4096), 0);
    assert.eq(testI32RemS_512(4097), 1);
    assert.eq(testI32RemS_512(-1), -1);
    assert.eq(testI32RemS_512(-512), 0);
    assert.eq(testI32RemS_512(-4095), -511);
    assert.eq(testI32RemS_512(-4096), 0);
    assert.eq(testI32RemS_512(-4097), -1);

    assert.eq(testI64RemU_512(0n), 0n);
    assert.eq(testI64RemU_512(1n), 1n);
    assert.eq(testI64RemU_512(512n), 0n);
    assert.eq(testI64RemU_512(4095n), 511n);
    assert.eq(testI64RemU_512(4096n), 0n);
    assert.eq(testI64RemU_512(4097n), 1n);

    assert.eq(testI64RemS_512(0n), 0n);
    assert.eq(testI64RemS_512(1n), 1n);
    assert.eq(testI64RemS_512(512n), 0n);
    assert.eq(testI64RemS_512(4095n), 511n);
    assert.eq(testI64RemS_512(4096n), 0n);
    assert.eq(testI64RemS_512(4097n), 1n);
    assert.eq(testI64RemS_512(-1n), -1n);
    assert.eq(testI64RemS_512(-512n), 0n);
    assert.eq(testI64RemS_512(-4095n), -511n);
    assert.eq(testI64RemS_512(-4096n), 0n);
    assert.eq(testI64RemS_512(-4097n), -1n);

    assert.eq(testI32RemU_131072(0), 0);
    assert.eq(testI32RemU_131072(1), 1);
    assert.eq(testI32RemU_131072(131072), 0);
    assert.eq(testI32RemU_131072(2097151), 131071);
    assert.eq(testI32RemU_131072(2097152), 0);
    assert.eq(testI32RemU_131072(2097153), 1);

    assert.eq(testI32RemS_131072(0), 0);
    assert.eq(testI32RemS_131072(1), 1);
    assert.eq(testI32RemS_131072(131072), 0);
    assert.eq(testI32RemS_131072(2097151), 131071);
    assert.eq(testI32RemS_131072(2097152), 0);
    assert.eq(testI32RemS_131072(2097153), 1);
    assert.eq(testI32RemS_131072(-1), -1);
    assert.eq(testI32RemS_131072(-131072), 0);
    assert.eq(testI32RemS_131072(-2097151), -131071);
    assert.eq(testI32RemS_131072(-2097152), 0);
    assert.eq(testI32RemS_131072(-2097153), -1);

    assert.eq(testI64RemU_131072(0n), 0n);
    assert.eq(testI64RemU_131072(1n), 1n);
    assert.eq(testI64RemU_131072(131072n), 0n);
    assert.eq(testI64RemU_131072(2097151n), 131071n);
    assert.eq(testI64RemU_131072(2097152n), 0n);
    assert.eq(testI64RemU_131072(2097153n), 1n);

    assert.eq(testI64RemS_131072(0n), 0n);
    assert.eq(testI64RemS_131072(1n), 1n);
    assert.eq(testI64RemS_131072(131072n), 0n);
    assert.eq(testI64RemS_131072(2097151n), 131071n);
    assert.eq(testI64RemS_131072(2097152n), 0n);
    assert.eq(testI64RemS_131072(2097153n), 1n);
    assert.eq(testI64RemS_131072(-1n), -1n);
    assert.eq(testI64RemS_131072(-131072n), 0n);
    assert.eq(testI64RemS_131072(-2097151n), -131071n);
    assert.eq(testI64RemS_131072(-2097152n), 0n);
    assert.eq(testI64RemS_131072(-2097153n), -1n);

    assert.eq(testI32RemU_134217728(0), 0);
    assert.eq(testI32RemU_134217728(1), 1);
    assert.eq(testI32RemU_134217728(134217728), 0);
    assert.eq(testI32RemU_134217728(536870911), 134217727);
    assert.eq(testI32RemU_134217728(536870912), 0);
    assert.eq(testI32RemU_134217728(536870913), 1);

    assert.eq(testI32RemS_134217728(0), 0);
    assert.eq(testI32RemS_134217728(1), 1);
    assert.eq(testI32RemS_134217728(134217728), 0);
    assert.eq(testI32RemS_134217728(536870911), 134217727);
    assert.eq(testI32RemS_134217728(536870912), 0);
    assert.eq(testI32RemS_134217728(536870913), 1);
    assert.eq(testI32RemS_134217728(-1), -1);
    assert.eq(testI32RemS_134217728(-134217728), 0);
    assert.eq(testI32RemS_134217728(-536870911), -134217727);
    assert.eq(testI32RemS_134217728(-536870912), 0);
    assert.eq(testI32RemS_134217728(-536870913), -1);

    assert.eq(testI64RemU_134217728(0n), 0n);
    assert.eq(testI64RemU_134217728(1n), 1n);
    assert.eq(testI64RemU_134217728(134217728n), 0n);
    assert.eq(testI64RemU_134217728(536870911n), 134217727n);
    assert.eq(testI64RemU_134217728(536870912n), 0n);
    assert.eq(testI64RemU_134217728(536870913n), 1n);

    assert.eq(testI64RemS_134217728(0n), 0n);
    assert.eq(testI64RemS_134217728(1n), 1n);
    assert.eq(testI64RemS_134217728(134217728n), 0n);
    assert.eq(testI64RemS_134217728(536870911n), 134217727n);
    assert.eq(testI64RemS_134217728(536870912n), 0n);
    assert.eq(testI64RemS_134217728(536870913n), 1n);
    assert.eq(testI64RemS_134217728(-1n), -1n);
    assert.eq(testI64RemS_134217728(-134217728n), 0n);
    assert.eq(testI64RemS_134217728(-536870911n), -134217727n);
    assert.eq(testI64RemS_134217728(-536870912n), 0n);
    assert.eq(testI64RemS_134217728(-536870913n), -1n);

    assert.eq(testI64RemU_8796093022208(0n), 0n);
    assert.eq(testI64RemU_8796093022208(1n), 1n);
    assert.eq(testI64RemU_8796093022208(8796093022208n), 0n);
    assert.eq(testI64RemU_8796093022208(562949953421311n), 8796093022207n);
    assert.eq(testI64RemU_8796093022208(562949953421312n), 0n);
    assert.eq(testI64RemU_8796093022208(562949953421313n), 1n);

    assert.eq(testI64RemS_8796093022208(0n), 0n);
    assert.eq(testI64RemS_8796093022208(1n), 1n);
    assert.eq(testI64RemS_8796093022208(8796093022208n), 0n);
    assert.eq(testI64RemS_8796093022208(562949953421311n), 8796093022207n);
    assert.eq(testI64RemS_8796093022208(562949953421312n), 0n);
    assert.eq(testI64RemS_8796093022208(562949953421313n), 1n);
    assert.eq(testI64RemS_8796093022208(-1n), -1n);
    assert.eq(testI64RemS_8796093022208(-8796093022208n), 0n);
    assert.eq(testI64RemS_8796093022208(-562949953421311n), -8796093022207n);
    assert.eq(testI64RemS_8796093022208(-562949953421312n), 0n);
    assert.eq(testI64RemS_8796093022208(-562949953421313n), -1n);
}

await assert.asyncTest(test());
