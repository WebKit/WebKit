// Regression test: Wasm Global must retain transitive TypeDefinition dependencies.
// A global typed (ref null $f) where $f references struct type $s must keep $s alive
// even after the module is collected.

import { instantiate } from "./wast-wrapper.js";

(function() {
    let inst = instantiate(`
        (module
            (rec (type $s (struct (field i32) (field i32) (field i32) (field i32))))
            (rec (type $f (func (param (ref null $s)) (result (ref null $s)))))
            (global (export "g") (mut (ref null $f)) (ref.null $f))
        )
    `);

    let g = inst.exports.g;
    inst = null;

    gc();
    gc();

    g.value;
    g.value = null;
})();
