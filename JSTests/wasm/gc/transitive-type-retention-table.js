// Regression test: Wasm Table must retain transitive TypeDefinition dependencies.
// A table typed (ref null $f) where $f references struct type $s must keep $s alive
// even after the module is collected.

import { instantiate } from "./wast-wrapper.js";

(function() {
    let inst = instantiate(`
        (module
            (rec (type $s (struct (field i32) (field i32) (field i32) (field i32))))
            (rec (type $f (func (param (ref null $s)) (result (ref null $s)))))
            (table (export "tbl") 10 10 (ref null $f))
        )
    `);

    let tbl = inst.exports.tbl;
    inst = null;

    gc();
    gc();

    tbl.get(0);
    tbl.set(0, null);
    tbl.grow(0, null);
})();
