//@ skip if $architecture == "arm"
import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

const maxArities = 64;

async function paramForwarder(numParams, paramType, value, imports) {
    let body = "";
    let inlineCheck = "";
    for (let i = 0; i < numParams; ++i) {
        body += `(local.get ${i + 1}) (call 0)\n`;
        inlineCheck += `(i32.lt_u (i32.const ${i + 1}) (local.get 0)) (if
        (then (br_if 0 (${paramType}.ne (local.get ${i + 1}) (${paramType}.const ${value}))))
        (else (br_if 0 (${paramType}.eq (local.get ${i + 1}) (local.get ${i + 1}))))
      )
`;
    }

    let wat = `
(module
  (func (import "env" "check") (param ${paramType}))
  (func (export "func") (param i32 ${Array(numParams).fill(paramType).join(" ")})
    (block
      ${body}
      ${inlineCheck}
      return
    )
    unreachable
  )
)
`;

    return await instantiate(wat, imports);
}

async function test () {
    for (let wasmArity = 20; wasmArity < maxArities; ++wasmArity) {
        let callerArity;
        let numChecked = 0;

        const check = value => {
            assert.isNumber(value);
            if (callerArity <= wasmArity) {
                if (numChecked < callerArity)
                    assert.eq(value, 64);
                else
                    assert.eq(value, NaN);
            } else {
                if (numChecked < wasmArity)
                    assert.eq(value, 64);
                else
                    asseert.eq(value, NaN);
            }
            ++numChecked;
        }

        const instance = await paramForwarder(wasmArity, "f64", 64, {env: {check}});
        for (callerArity = 0; callerArity < maxArities; ++callerArity) {
            const params = Array(callerArity + 1).fill(64);
            params[0] = callerArity;
            instance.exports.func(...params);
            numChecked = 0;
        }
    }
}

await assert.asyncTest(test());
