import * as assert from "../assert.js";

async function test() {
    const wat = `
        (module
            (type $sig_l (func (result i64)))
            (type $sig_l_l (func (param i64) (result i64)))
            (func (export "c_l") (type $sig_l) (i64.const 0))
            (func (export "id_l") (type $sig_l_l) (local.get 0))
            (table (export "table") 1 2 (ref null $sig_l))
        )
    `;

    let buffer = new Uint8Array([0,97,115,109,1,0,0,0,1,10,2,96,1,126,1,126,96,0,1,126,3,3,2,0,1,4,6,1,99,1,1,1,2,7,22,3,5,116,97,98,108,101,1,0,4,105,100,95,108,0,0,3,99,95,108,0,1,10,11,2,4,0,32,0,11,4,0,66,0,11,0,19,4,110,97,109,101,1,12,2,0,4,105,100,95,108,1,3,99,95,108]);
    let instance = await WebAssembly.instantiate(buffer);
    let { table, id_l, c_l } = instance.instance.exports;

    assert.throws(
      () => table.set(0, id_l),
      TypeError,
      "Argument value did not match the reference type"
    );

    assert.throws(
      () => table.grow(1, id_l),
      TypeError,
      "Argument value did not match the reference type"
    );

    table.set(0, c_l);
    table.grow(1, c_l);
}

assert.asyncTest(test());