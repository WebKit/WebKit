//@ skip if $hostOS == "linux"
//@ slow!
// https://bugs.webkit.org/show_bug.cgi?id=247454
// https://bugs.webkit.org/show_bug.cgi?id=320559
import * as assert from "../assert.js";
import { compile, instantiate } from "../gc/wast-wrapper.js";

function testConcreteVsAbstract() {
    const m = instantiate(`
      (module
        (type $S (sub (struct (field i32))))
        (type $T (sub $S (struct (field i32) (field i64))))
        (func (export "asAny") (result anyref)
          (struct.new $T (i32.const 1) (i64.const 2)))
        (func (export "asStruct") (result structref)
          (struct.new $S (i32.const 3)))
        (func (export "castToS") (param anyref) (result (ref null $S))
          (ref.cast (ref null $S) (local.get 0)))
        (func (export "testIsS") (param anyref) (result i32)
          (ref.test (ref null $S) (local.get 0)))
      )
    `).exports;

    const v = m.asAny();
    assert.eq(m.testIsS(v), 1);
    m.castToS(v);
    m.castToS(m.asStruct());
    assert.eq(m.testIsS(null), 1);
    assert.eq(m.testIsS(42), 0);
}

for (let i = 0; i < wasmTestLoopCount; ++i)
    testConcreteVsAbstract();
