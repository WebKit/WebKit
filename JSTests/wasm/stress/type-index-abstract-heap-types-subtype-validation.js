//@ skip if $hostOS == "linux"
//@ slow!
// https://bugs.webkit.org/show_bug.cgi?id=247454
// https://bugs.webkit.org/show_bug.cgi?id=320559
import * as assert from "../assert.js";
import { compile, instantiate } from "../gc/wast-wrapper.js";

function testSubtypeValidation() {
    compile(`
      (module
        (type $S (struct))
        (func (param (ref null $S)) (result)
          (drop (local.get 0)))
        (func (param anyref) (result)
          (call 0 (ref.cast (ref null $S) (local.get 0)))))
    `);

    assert.throws(
        () => compile(`
          (module
            (func (param funcref) (result)
              (drop (ref.cast externref (local.get 0)))))
        `),
        WebAssembly.CompileError,
        "ref.cast"
    );

    assert.throws(
        () => compile(`
          (module
            (func (param externref) (result anyref)
              (ref.cast anyref (local.get 0))))
        `),
        WebAssembly.CompileError,
        "ref.cast"
    );

    compile(`
      (module
        (func (param i31ref) (result anyref)
          (ref.cast anyref (local.get 0))))
    `);

    compile(`
      (module
        (func (param structref) (result eqref)
          (ref.cast eqref (local.get 0))))
    `);
}

for (let i = 0; i < wasmTestLoopCount; ++i)
    testSubtypeValidation();
