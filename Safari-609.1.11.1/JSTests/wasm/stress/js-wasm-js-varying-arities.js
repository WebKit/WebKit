import { instantiate } from "../wabt-wrapper.js";
import * as assert from "../assert.js";

const maxArities = 64;

function paramForwarder(numParams, paramType, value, imports) {
    let body = "";
    let inlineCheck = "";
    for (let i = 0; i < numParams; ++i) {
        body += `(get_local ${i + 1}) (call 0)\n`;
        inlineCheck += `(i32.lt_u (i32.const ${i + 1}) (get_local 0)) (if
        (then (br_if 0 (${paramType}.ne (get_local ${i + 1}) (${paramType}.const ${value}))))
        (else (br_if 0 (${paramType}.eq (get_local ${i + 1}) (get_local ${i + 1}))))
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

    return instantiate(wat, imports);
}

for (let wasmArity = 20;  wasmArity < maxArities; ++wasmArity) {
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

    const instance = paramForwarder(wasmArity, "f64", 64, { env: { check } });
    for (callerArity = 0; callerArity < maxArities; ++callerArity) {
        const params = Array(callerArity + 1).fill(64);
        params[0] = callerArity;
        const result = instance.exports.func(...params);
        numChecked = 0;
    }
}
