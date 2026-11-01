//@ requireOptions("--useWasmJSTypes=true")
import * as assert from "../assert.js";
import { instantiate } from "../wabt-wrapper.js";

{
    const fun = new WebAssembly.Function({ parameters: [], results: ["i32"] }, () => 7);
    const instance = await instantiate(`
        (module
          (import "m" "fun" (func $fun (result i32)))
          (func (export "main") (result i32)
            (call $fun))
          (export "fun1" (func $fun)))
    `, { m: { fun } });
    assert.eq(instance.exports.main(), 7);
    assert.eq(instance.exports.fun1, fun);
}

{
    const instance = await instantiate(`
        (module
          (func (export "f") (result i32)
            (i32.const 1)))
    `);
    assert.eq(new WebAssembly.Function({ parameters: [], results: ["i32"] }, instance.exports.f), instance.exports.f);
    assert.throws(() => new WebAssembly.Function({ parameters: ["i32"], results: [] }, instance.exports.f), TypeError, "");
}
