import * as assert from "../assert.js";
import { compile, instantiate } from "./wast-wrapper.js";

let wat = `
(module
    (type $S1 (struct (field $vec (mut v128))))

    (func (export "leak") (result i64)
        (struct.new_default $S1)
        (struct.get $S1 0)
        (i64x2.extract_lane 1)
    )
)`;

function test() {
    const instance = instantiate(wat);

    for (let i = 0; i < 1000000; i++) {
        const value = instance.exports.leak();
        assert.eq(value, 0n);
    }
}

test();
