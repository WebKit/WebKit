//@ requireOptions("--useWasmJSTypes=true")
import * as assert from "../assert.js";
import { instantiate } from "../wabt-wrapper.js";
import { instantiate as instantiateWast } from "../gc/wast-wrapper.js";

{
    const table = new WebAssembly.Table({ element: "anyfunc", initial: 1 });
    const fun = new WebAssembly.Function({ parameters: [], results: ["i32"] }, () => 42);
    table.set(0, fun);
    const instance = await instantiate(`
        (module
          (import "m" "table" (table 1 funcref))
          (type $t (func (result i32)))
          (func (export "main") (result i32)
            (call_indirect (type $t) (i32.const 0))))
    `, { m: { table } });
    for (let i = 0; i < 10; ++i)
        assert.eq(instance.exports.main(), 42);
}

{
    const fun = new WebAssembly.Function({ parameters: [], results: ["i32"] }, () => 42);
    const instance = instantiateWast(`
        (module
          (type $t (func (result i32)))
          (func (export "main") (param $f funcref) (result i32)
            (call_ref $t (ref.cast (ref $t) (local.get $f)))))
    `);
    for (let i = 0; i < 10; ++i)
        assert.eq(instance.exports.main(fun), 42);
}

{
    const table = new WebAssembly.Table({ element: "anyfunc", initial: 1 });
    const fun = new WebAssembly.Function({ parameters: [], results: ["i32"] }, () => 42);
    table.set(0, fun);
    const instance = instantiateWast(`
        (module
          (import "m" "table" (table 1 funcref))
          (type $t (func (result i32)))
          (func (export "main") (result i32)
            (return_call_indirect (type $t) (i32.const 0))))
    `, { m: { table } });
    for (let i = 0; i < 10; ++i)
        assert.eq(instance.exports.main(), 42);
}

{
    const fun = new WebAssembly.Function({ parameters: [], results: ["i32"] }, () => 42);
    const instance = instantiateWast(`
        (module
          (type $t (func (result i32)))
          (func (export "main") (param $f funcref) (result i32)
            (return_call_ref $t (ref.cast (ref $t) (local.get $f)))))
    `);
    for (let i = 0; i < 10; ++i)
        assert.eq(instance.exports.main(fun), 42);
}
