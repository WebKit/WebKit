import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

const count = 256;
const input = 0x0fedcba987654321n;

function constant(index)
{
    return -0x12345600n - BigInt(index) * 0x10101n;
}

const operations = {
    and: (a, b) => a & b,
    or: (a, b) => a | b,
    xor: (a, b) => a ^ b,
};

let wat = `(module`;
for (const name in operations) {
    wat += `(func (export "${name}") (param i64) (result i64) i64.const 0`;
    for (let i = 0; i < count; ++i)
        wat += ` local.get 0 i64.const ${constant(i)} i64.${name} i64.add`;
    wat += `)`;
}
wat += `)`;

const instance = await instantiate(wat);

for (const name in operations) {
    let expected = 0n;
    for (let i = 0; i < count; ++i)
        expected += operations[name](input, constant(i));
    expected = BigInt.asIntN(64, expected);

    for (let i = 0; i < wasmTestLoopCount; ++i)
        assert.eq(instance.exports[name](input), expected);
}
