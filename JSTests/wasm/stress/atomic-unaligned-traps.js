import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

const verbose = false;

function genAtomicInstr(op, subOp, typeSz, accessSz) {
    let opSz = '';

    if (subOp != '')
        subOp = '.' + subOp; // rmw ops

    if (typeSz != accessSz) {
        opSz = `${accessSz}`;
        if (op != 'store')
            subOp += '_u';
        }
    return `i${typeSz}.atomic.${op}${opSz}${subOp}`
}

function genWat(op, subOp, typeSz, accessSz) {
    const instr = genAtomicInstr(op, subOp, typeSz, accessSz);
    let expected = '', operand = '', retVal = '';
    if (op == 'store') {
        operand = `i${typeSz}.const 42`;
        retVal = `i${typeSz}.const 7`;
    } else if (op == 'rmw') {
        operand = `i${typeSz}.const 80`;
        if (subOp == 'cmpxchg')
            expected = `i${typeSz}.const 13`;
    }
    let wat = `
    (module
      (func (export "test") (param $addr i32) (result i${typeSz})
        local.get $addr
        ${expected}
        ${operand}
        ${instr}
        ${retVal}
      )
      (memory 1)
    )
    `;
    if (verbose)
        print(wat + '\n');
    return wat;
}

async function test(op, subOp, typeSz, accessSz) {
    const instance = await instantiate(genWat(op, subOp, typeSz, accessSz), {}, {threads: true});
    const {test} = instance.exports;
    assert.throws(() => {
        test(1);
    }, WebAssembly.RuntimeError, `Unaligned memory access`);
}

for (const op of ['load', 'store']) {
    for (const typeSz of [32, 64]) {
        for (let accessSz = typeSz; accessSz >= 16; accessSz /= 2)
            await assert.asyncTest(test(op, '', typeSz, accessSz));
    }
}

// RMW operators
for (const subOp of ['add', 'sub', 'and', 'or', 'xor', 'xchg', 'cmpxchg']) {
    for (const typeSz of [32, 64]) {
        for (let accessSz = typeSz; accessSz >= 16; accessSz /= 2)
            await assert.asyncTest(test('rmw', subOp, typeSz, accessSz));
    }
}
