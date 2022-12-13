//@ requireOptions("--webAssemblyBBQAirModeThreshold=1")
//@ skip if $architecture == "arm"

// Regression test for https://bugs.webkit.org/show_bug.cgi?id=242294
// Run stress/osr-entry-basic.js with BBQB3 mode.

import Builder from '../Builder.js'
import * as assert from '../assert.js'

{
    const b = new Builder();
    b.Type().End()
        .Function().End()
        .Export()
            .Function("loop")
        .End()
        .Code()
        .Function("loop", { params: ["i32"], ret: "i32" }, ["i32"])
        .I32Const(0)
        .SetLocal(1)
        .Loop("void")
        .Block("void", b =>
               b.GetLocal(0)
               .I32Const(0)
               .I32Eq()
               .BrIf(0)
               .GetLocal(0)
               .GetLocal(1)
               .I32Add()
               .SetLocal(1)
               .GetLocal(0)
               .I32Const(1)
               .I32Sub()
               .SetLocal(0)
               .Br(1)
              )
        .End()
        .GetLocal(1)
        .Return()
        .End()
        .End()

    const bin = b.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    const instance = new WebAssembly.Instance(module);

    assert.eq(987459712, instance.exports.loop(100000000));
}
