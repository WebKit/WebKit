import * as assert from "../assert.js";

/*
(module
  (import "env" "throw" (func $jsThrow))

  (func $callRethrow
    (try $try
      (do
        (call $jsThrow)
      )

      (catch_all
        (rethrow $try)
      )
    )
  )

  (func $throwRef (param $exnref exnref)
    (throw_ref (local.get $exnref))
  )

  (func (export "trigger")
    (call $throwRef (block $block (result exnref)
      (try_table (catch_all_ref $block)
        (call $callRethrow)
        (return)
      )
    ))
  )
)
*/
const WASM_MODULE_CODE = new Uint8Array([0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x08, 0x02, 0x60, 0x00, 0x00, 0x60, 0x01, 0x69, 0x00, 0x02, 0x0d, 0x01, 0x03, 0x65, 0x6e, 0x76, 0x05, 0x74, 0x68, 0x72, 0x6f, 0x77, 0x00, 0x00, 0x03, 0x04, 0x03, 0x00, 0x01, 0x00, 0x07, 0x0b, 0x01, 0x07, 0x74, 0x72, 0x69, 0x67, 0x67, 0x65, 0x72, 0x00, 0x03, 0x0a, 0x24, 0x03, 0x0a, 0x00, 0x06, 0x40, 0x10, 0x00, 0x19, 0x09, 0x00, 0x0b, 0x0b, 0x05, 0x00, 0x20, 0x00, 0x0a, 0x0b, 0x11, 0x00, 0x02, 0x69, 0x1f, 0x40, 0x01, 0x03, 0x00, 0x10, 0x01, 0x0f, 0x0b, 0x00, 0x0b, 0x10, 0x02, 0x0b]);

function main() {
    let shouldThrow = false;
    const instance = new WebAssembly.Instance(new WebAssembly.Module(WASM_MODULE_CODE), {
        env: {
            throw() {
                if (shouldThrow) {
                    throw 1234;
                }
            }
        }
    });

    for (let i = 0; i < 1000; i++) {
        instance.exports.trigger()
    }

    shouldThrow = true;
    instance.exports.trigger();
}

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

try {
    main();
} catch (error) {
    assert.eq(error.message, "Module uses both legacy exceptions and try_table (evaluating 'new WebAssembly.Module(WASM_MODULE_CODE)')");
}
