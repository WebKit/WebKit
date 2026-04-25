import { instantiate } from "../wabt-wrapper.js";
import * as assert from '../assert.js';

(async function () {
    let wat = `
        (module
            (func $callee (import "env" "callee") (result i32 i32))
            (func (export "func") (result i32 i32) (call $callee))
        )
        `;
    let flag = true;
    let instance = await instantiate(wat, {
            env: {
                callee: function ()
                {
                    if (flag)
                        return [32];
                    return [32, 32, 32, 32];
                }
            }
        }, {});
    assert.throws(() => {
        instance.exports.func();
    }, TypeError, `Incorrect number of values returned to Wasm from JS`);
    flag = false;
    assert.throws(() => {
        instance.exports.func();
    }, TypeError, `Incorrect number of values returned to Wasm from JS`);
}()).catch($vm.abort);
