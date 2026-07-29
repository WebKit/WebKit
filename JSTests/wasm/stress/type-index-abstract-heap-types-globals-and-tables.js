//@ skip if $hostOS == "linux"
//@ slow!
// https://bugs.webkit.org/show_bug.cgi?id=247454
// https://bugs.webkit.org/show_bug.cgi?id=320559
import * as assert from "../assert.js";
import { compile, instantiate } from "../gc/wast-wrapper.js";

function testGlobalsAndTables() {
    const m = instantiate(`
      (module
        (global $g (mut anyref) (ref.null any))
        (global $gf (export "gf") funcref (ref.null func))
        (table $t 2 anyref)
        (func (export "setGlobal") (param anyref)
          (global.set $g (local.get 0)))
        (func (export "getGlobal") (result anyref)
          (global.get $g))
        (func (export "setTable") (param i32 anyref)
          (table.set $t (local.get 0) (local.get 1)))
        (func (export "getTable") (param i32) (result anyref)
          (table.get $t (local.get 0)))
        (func (export "nullInTable") (result i32)
          (table.set $t (i32.const 0) (ref.null none))
          (ref.is_null (table.get $t (i32.const 0))))
      )
    `).exports;

    assert.eq(m.gf.value, null);
    m.setGlobal(null);
    assert.eq(m.getGlobal(), null);
    m.setTable(1, "hello");
    assert.eq(m.getTable(1), "hello");
    assert.eq(m.nullInTable(), 1);
}

for (let i = 0; i < wasmTestLoopCount; ++i)
    testGlobalsAndTables();
