import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

let wat = `
(module
    (func (export "testI32DivU_2") (param i32) (result i32)
        local.get 0
        i32.const 2
        i32.div_u
    )

    (func (export "testI32DivS_2") (param i32) (result i32)
        local.get 0
        i32.const 2
        i32.div_s
    )

    (func (export "testI32DivU_32") (param i32) (result i32)
        local.get 0
        i32.const 32
        i32.div_u
    )

    (func (export "testI32DivS_32") (param i32) (result i32)
        local.get 0
        i32.const 32
        i32.div_s
    )

    (func (export "testI32DivU_512") (param i32) (result i32)
        local.get 0
        i32.const 512
        i32.div_u
    )

    (func (export "testI32DivS_512") (param i32) (result i32)
        local.get 0
        i32.const 512
        i32.div_s
    )

    (func (export "testI32DivU_131072") (param i32) (result i32)
        local.get 0
        i32.const 131072
        i32.div_u
    )

    (func (export "testI32DivS_131072") (param i32) (result i32)
        local.get 0
        i32.const 131072
        i32.div_s
    )

    (func (export "testI32DivU_134217728") (param i32) (result i32)
        local.get 0
        i32.const 134217728
        i32.div_u
    )

    (func (export "testI32DivS_134217728") (param i32) (result i32)
        local.get 0
        i32.const 134217728
        i32.div_s
    )

    (func (export "testI64DivU_2") (param i64) (result i64)
        local.get 0
        i64.const 2
        i64.div_u
    )

    (func (export "testI64DivS_2") (param i64) (result i64)
        local.get 0
        i64.const 2
        i64.div_s
    )

    (func (export "testI64DivU_32") (param i64) (result i64)
        local.get 0
        i64.const 32
        i64.div_u
    )

    (func (export "testI64DivS_32") (param i64) (result i64)
        local.get 0
        i64.const 32
        i64.div_s
    )

    (func (export "testI64DivU_512") (param i64) (result i64)
        local.get 0
        i64.const 512
        i64.div_u
    )

    (func (export "testI64DivS_512") (param i64) (result i64)
        local.get 0
        i64.const 512
        i64.div_s
    )

    (func (export "testI64DivU_131072") (param i64) (result i64)
        local.get 0
        i64.const 131072
        i64.div_u
    )

    (func (export "testI64DivS_131072") (param i64) (result i64)
        local.get 0
        i64.const 131072
        i64.div_s
    )

    (func (export "testI64DivU_134217728") (param i64) (result i64)
        local.get 0
        i64.const 134217728
        i64.div_u
    )

    (func (export "testI64DivS_134217728") (param i64) (result i64)
        local.get 0
        i64.const 134217728
        i64.div_s
    )

    (func (export "testI64DivU_8796093022208") (param i64) (result i64)
        local.get 0
        i64.const 8796093022208
        i64.div_u
    )

    (func (export "testI64DivS_8796093022208") (param i64) (result i64)
        local.get 0
        i64.const 8796093022208
        i64.div_s
    )
)
`;

async function test() {
    const instance = await instantiate(wat, {}, {});
    const { 
        testI32DivU_2,
        testI32DivS_2,
        testI32DivU_32,
        testI32DivS_32,
        testI32DivU_512,
        testI32DivS_512,
        testI32DivU_131072,
        testI32DivS_131072,
        testI32DivU_134217728,
        testI32DivS_134217728,
        testI64DivU_2,
        testI64DivS_2,
        testI64DivU_32,
        testI64DivS_32,
        testI64DivU_512,
        testI64DivS_512,
        testI64DivU_131072,
        testI64DivS_131072,
        testI64DivU_134217728,
        testI64DivS_134217728,
        testI64DivU_8796093022208,
        testI64DivS_8796093022208
    } = instance.exports;

    assert.eq(testI32DivU_2(0), 0);
    assert.eq(testI32DivU_2(1), 0);
    assert.eq(testI32DivU_2(2), 1);
    assert.eq(testI32DivU_2(7), 3);
    assert.eq(testI32DivU_2(8), 4);
    assert.eq(testI32DivU_2(9), 4);

    assert.eq(testI32DivS_2(0), 0);
    assert.eq(testI32DivS_2(1), 0);
    assert.eq(testI32DivS_2(2), 1);
    assert.eq(testI32DivS_2(7), 3);
    assert.eq(testI32DivS_2(8), 4);
    assert.eq(testI32DivS_2(9), 4);
    assert.eq(testI32DivS_2(-1), 0);
    assert.eq(testI32DivS_2(-2), -1);
    assert.eq(testI32DivS_2(-7), -3);
    assert.eq(testI32DivS_2(-8), -4);
    assert.eq(testI32DivS_2(-9), -4);

    assert.eq(testI64DivU_2(0n), 0n);
    assert.eq(testI64DivU_2(1n), 0n);
    assert.eq(testI64DivU_2(2n), 1n);
    assert.eq(testI64DivU_2(7n), 3n);
    assert.eq(testI64DivU_2(8n), 4n);
    assert.eq(testI64DivU_2(9n), 4n);

    assert.eq(testI64DivS_2(0n), 0n);
    assert.eq(testI64DivS_2(1n), 0n);
    assert.eq(testI64DivS_2(2n), 1n);
    assert.eq(testI64DivS_2(7n), 3n);
    assert.eq(testI64DivS_2(8n), 4n);
    assert.eq(testI64DivS_2(9n), 4n);
    assert.eq(testI64DivS_2(-1n), 0n);
    assert.eq(testI64DivS_2(-2n), -1n);
    assert.eq(testI64DivS_2(-7n), -3n);
    assert.eq(testI64DivS_2(-8n), -4n);
    assert.eq(testI64DivS_2(-9n), -4n);

    assert.eq(testI32DivU_32(0), 0);
    assert.eq(testI32DivU_32(1), 0);
    assert.eq(testI32DivU_32(32), 1);
    assert.eq(testI32DivU_32(127), 3);
    assert.eq(testI32DivU_32(128), 4);
    assert.eq(testI32DivU_32(129), 4);

    assert.eq(testI32DivS_32(0), 0);
    assert.eq(testI32DivS_32(1), 0);
    assert.eq(testI32DivS_32(32), 1);
    assert.eq(testI32DivS_32(127), 3);
    assert.eq(testI32DivS_32(128), 4);
    assert.eq(testI32DivS_32(129), 4);
    assert.eq(testI32DivS_32(-1), 0);
    assert.eq(testI32DivS_32(-32), -1);
    assert.eq(testI32DivS_32(-127), -3);
    assert.eq(testI32DivS_32(-128), -4);
    assert.eq(testI32DivS_32(-129), -4);

    assert.eq(testI64DivU_32(0n), 0n);
    assert.eq(testI64DivU_32(1n), 0n);
    assert.eq(testI64DivU_32(32n), 1n);
    assert.eq(testI64DivU_32(127n), 3n);
    assert.eq(testI64DivU_32(128n), 4n);
    assert.eq(testI64DivU_32(129n), 4n);

    assert.eq(testI64DivS_32(0n), 0n);
    assert.eq(testI64DivS_32(1n), 0n);
    assert.eq(testI64DivS_32(32n), 1n);
    assert.eq(testI64DivS_32(127n), 3n);
    assert.eq(testI64DivS_32(128n), 4n);
    assert.eq(testI64DivS_32(129n), 4n);
    assert.eq(testI64DivS_32(-1n), 0n);
    assert.eq(testI64DivS_32(-32n), -1n);
    assert.eq(testI64DivS_32(-127n), -3n);
    assert.eq(testI64DivS_32(-128n), -4n);
    assert.eq(testI64DivS_32(-129n), -4n);

    assert.eq(testI32DivU_512(0), 0);
    assert.eq(testI32DivU_512(1), 0);
    assert.eq(testI32DivU_512(512), 1);
    assert.eq(testI32DivU_512(4095), 7);
    assert.eq(testI32DivU_512(4096), 8);
    assert.eq(testI32DivU_512(4097), 8);

    assert.eq(testI32DivS_512(0), 0);
    assert.eq(testI32DivS_512(1), 0);
    assert.eq(testI32DivS_512(512), 1);
    assert.eq(testI32DivS_512(4095), 7);
    assert.eq(testI32DivS_512(4096), 8);
    assert.eq(testI32DivS_512(4097), 8);
    assert.eq(testI32DivS_512(-1), 0);
    assert.eq(testI32DivS_512(-512), -1);
    assert.eq(testI32DivS_512(-4095), -7);
    assert.eq(testI32DivS_512(-4096), -8);
    assert.eq(testI32DivS_512(-4097), -8);

    assert.eq(testI64DivU_512(0n), 0n);
    assert.eq(testI64DivU_512(1n), 0n);
    assert.eq(testI64DivU_512(512n), 1n);
    assert.eq(testI64DivU_512(4095n), 7n);
    assert.eq(testI64DivU_512(4096n), 8n);
    assert.eq(testI64DivU_512(4097n), 8n);

    assert.eq(testI64DivS_512(0n), 0n);
    assert.eq(testI64DivS_512(1n), 0n);
    assert.eq(testI64DivS_512(512n), 1n);
    assert.eq(testI64DivS_512(4095n), 7n);
    assert.eq(testI64DivS_512(4096n), 8n);
    assert.eq(testI64DivS_512(4097n), 8n);
    assert.eq(testI64DivS_512(-1n), 0n);
    assert.eq(testI64DivS_512(-512n), -1n);
    assert.eq(testI64DivS_512(-4095n), -7n);
    assert.eq(testI64DivS_512(-4096n), -8n);
    assert.eq(testI64DivS_512(-4097n), -8n);

    assert.eq(testI32DivU_131072(0), 0);
    assert.eq(testI32DivU_131072(1), 0);
    assert.eq(testI32DivU_131072(131072), 1);
    assert.eq(testI32DivU_131072(2097151), 15);
    assert.eq(testI32DivU_131072(2097152), 16);
    assert.eq(testI32DivU_131072(2097153), 16);

    assert.eq(testI32DivS_131072(0), 0);
    assert.eq(testI32DivS_131072(1), 0);
    assert.eq(testI32DivS_131072(131072), 1);
    assert.eq(testI32DivS_131072(2097151), 15);
    assert.eq(testI32DivS_131072(2097152), 16);
    assert.eq(testI32DivS_131072(2097153), 16);
    assert.eq(testI32DivS_131072(-1), 0);
    assert.eq(testI32DivS_131072(-131072), -1);
    assert.eq(testI32DivS_131072(-2097151), -15);
    assert.eq(testI32DivS_131072(-2097152), -16);
    assert.eq(testI32DivS_131072(-2097153), -16);

    assert.eq(testI64DivU_131072(0n), 0n);
    assert.eq(testI64DivU_131072(1n), 0n);
    assert.eq(testI64DivU_131072(131072n), 1n);
    assert.eq(testI64DivU_131072(2097151n), 15n);
    assert.eq(testI64DivU_131072(2097152n), 16n);
    assert.eq(testI64DivU_131072(2097153n), 16n);

    assert.eq(testI64DivS_131072(0n), 0n);
    assert.eq(testI64DivS_131072(1n), 0n);
    assert.eq(testI64DivS_131072(131072n), 1n);
    assert.eq(testI64DivS_131072(2097151n), 15n);
    assert.eq(testI64DivS_131072(2097152n), 16n);
    assert.eq(testI64DivS_131072(2097153n), 16n);
    assert.eq(testI64DivS_131072(-1n), 0n);
    assert.eq(testI64DivS_131072(-131072n), -1n);
    assert.eq(testI64DivS_131072(-2097151n), -15n);
    assert.eq(testI64DivS_131072(-2097152n), -16n);
    assert.eq(testI64DivS_131072(-2097153n), -16n);

    assert.eq(testI32DivU_134217728(0), 0);
    assert.eq(testI32DivU_134217728(1), 0);
    assert.eq(testI32DivU_134217728(134217728), 1);
    assert.eq(testI32DivU_134217728(536870911), 3);
    assert.eq(testI32DivU_134217728(536870912), 4);
    assert.eq(testI32DivU_134217728(536870913), 4);

    assert.eq(testI32DivS_134217728(0), 0);
    assert.eq(testI32DivS_134217728(1), 0);
    assert.eq(testI32DivS_134217728(134217728), 1);
    assert.eq(testI32DivS_134217728(536870911), 3);
    assert.eq(testI32DivS_134217728(536870912), 4);
    assert.eq(testI32DivS_134217728(536870913), 4);
    assert.eq(testI32DivS_134217728(-1), 0);
    assert.eq(testI32DivS_134217728(-134217728), -1);
    assert.eq(testI32DivS_134217728(-536870911), -3);
    assert.eq(testI32DivS_134217728(-536870912), -4);
    assert.eq(testI32DivS_134217728(-536870913), -4);

    assert.eq(testI64DivU_134217728(0n), 0n);
    assert.eq(testI64DivU_134217728(1n), 0n);
    assert.eq(testI64DivU_134217728(134217728n), 1n);
    assert.eq(testI64DivU_134217728(536870911n), 3n);
    assert.eq(testI64DivU_134217728(536870912n), 4n);
    assert.eq(testI64DivU_134217728(536870913n), 4n);

    assert.eq(testI64DivS_134217728(0n), 0n);
    assert.eq(testI64DivS_134217728(1n), 0n);
    assert.eq(testI64DivS_134217728(134217728n), 1n);
    assert.eq(testI64DivS_134217728(536870911n), 3n);
    assert.eq(testI64DivS_134217728(536870912n), 4n);
    assert.eq(testI64DivS_134217728(536870913n), 4n);
    assert.eq(testI64DivS_134217728(-1n), 0n);
    assert.eq(testI64DivS_134217728(-134217728n), -1n);
    assert.eq(testI64DivS_134217728(-536870911n), -3n);
    assert.eq(testI64DivS_134217728(-536870912n), -4n);
    assert.eq(testI64DivS_134217728(-536870913n), -4n);

    assert.eq(testI64DivU_8796093022208(0n), 0n);
    assert.eq(testI64DivU_8796093022208(1n), 0n);
    assert.eq(testI64DivU_8796093022208(8796093022208n), 1n);
    assert.eq(testI64DivU_8796093022208(562949953421311n), 63n);
    assert.eq(testI64DivU_8796093022208(562949953421312n), 64n);
    assert.eq(testI64DivU_8796093022208(562949953421313n), 64n);

    assert.eq(testI64DivS_8796093022208(0n), 0n);
    assert.eq(testI64DivS_8796093022208(1n), 0n);
    assert.eq(testI64DivS_8796093022208(8796093022208n), 1n);
    assert.eq(testI64DivS_8796093022208(562949953421311n), 63n);
    assert.eq(testI64DivS_8796093022208(562949953421312n), 64n);
    assert.eq(testI64DivS_8796093022208(562949953421313n), 64n);
    assert.eq(testI64DivS_8796093022208(-1n), 0n);
    assert.eq(testI64DivS_8796093022208(-8796093022208n), -1n);
    assert.eq(testI64DivS_8796093022208(-562949953421311n), -63n);
    assert.eq(testI64DivS_8796093022208(-562949953421312n), -64n);
    assert.eq(testI64DivS_8796093022208(-562949953421313n), -64n);
}

await assert.asyncTest(test());
