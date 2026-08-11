import * as assert from '../assert.js';
import { watToWasm } from "../gc/wast-wrapper.js";

const wat = String.raw `
(module
    (type $0 (sub (array (mut f64))))
    (data $0 "\fa\a1\03\b7\f7\87Cz\93G\df\b2\0c\e9\99\a0\ff\e3\9f\b5\85)\88\\J\10\f5\94\c8A\cf5\de\87I7o\06\8c2\14\a3q\84w\82\c1\15\f1\1f_\db\81C\81|6\cb\04#\8a")
    (func $main (export "main")
        (array.init_data $0 0
            ;; The return value of 'if' is declared nullable, but the expression in the 'else' branch is not.
            ;; That expression is left on the expression stack representing the result of 'if'.
            ;; Despite that, 'if' as a whole should be treated as nullable.
            (if (result (ref null $0))
                (i32.const 1)
                (then
                    (ref.cast nullref (ref.null none))
                )
                (else
                    (array.new_default $0 (i32.const 1))
                )
            )
            (i32.const 0)
            (i32.const 0)
            (i32.const 0)
        )
    )
)`;

async function test() {
    const buffer = await watToWasm(wat);
    const module = new WebAssembly.Module(buffer);
    const instance = new WebAssembly.Instance(module, {});

    for (let i = 0; i < wasmTestLoopCount; ++i) {
        // The generated code for array.init_data should include a null check so the null returned in the 'then' branch is detected as expected
        assert.throws(instance.exports.main, WebAssembly.RuntimeError, "array.init_data to a null reference");
    }
}

await test();
