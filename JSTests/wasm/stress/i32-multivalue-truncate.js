import * as assert from '../assert.js';
import { instantiate } from "../wabt-wrapper.js";

const wat = `
(module
  ;; Import JS function that returns two i32 values (to trigger multi-value path)
  (import "imports" "getTwoI32s" (func $getTwoI32s (result i32 i32)))

  (memory (export "memory") 1)

  (func (export "testDirectMemoryAccess") (result i32)
    call $getTwoI32s
    drop  ;; drop second value
    i32.load8_u  ;; use first value directly as offset
  )
)
`;

async function test() {
    function getTwoI32s() {
        return [-1, 99];  // First is -1, second is dummy to force multi-value result path
    }

    let instance = await instantiate(wat, { imports: { getTwoI32s: getTwoI32s }});

    for (let i = 0; i < wasmTestLoopCount; i++) {
        let trapped = false;
        try {
            instance.exports.testDirectMemoryAccess();
        } catch (e) {
            trapped = true;
            assert.truthy(e instanceof WebAssembly.RuntimeError);
        }
        assert.truthy(trapped, "Expected trap for -1 used as memory offset");
    }
}

await assert.asyncTest(test());
