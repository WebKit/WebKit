// @requireOptions("--useWasmTypedFunctionReferences=true")

import * as assert from "../assert.js"

// https://github.com/WebAssembly/function-references/blob/main/proposals/function-references/Overview.md#type-reflection

{
    // (module
    //     (table (export "t") 10 (ref func) (ref.func 0))
    //     (func)
    // )
    const bytes = new Uint8Array([0,97,115,109,1,0,0,0,1,4,1,96,0,0,3,2,1,0,4,10,1,64,0,100,112,0,10,210,0,11,7,5,1,1,116,1,0,10,4,1,2,0,11]);
    const module = new WebAssembly.Module(bytes);
    assert.throws(function () {
        WebAssembly.Module.exports(module);
    }, TypeError, "WebAssembly.Module.exports unable to produce export descriptors for the given module");
}

{
    // (module
    //     (global (export "g") (ref func) (ref.func 0))
    //     (func)
    // )
    const bytes = new Uint8Array([0,97,115,109,1,0,0,0,1,4,1,96,0,0,3,2,1,0,6,7,1,100,112,0,210,0,11,7,5,1,1,103,3,0,10,4,1,2,0,11]);
    const module = new WebAssembly.Module(bytes);
    assert.throws(function () {
        WebAssembly.Module.exports(module);
    }, TypeError, "WebAssembly.Module.exports unable to produce export descriptors for the given module");
}

{
    // (module
    //     (func (export "f") (param (ref func)))
    //     (func)
    // )
    const bytes = new Uint8Array([0,97,115,109,1,0,0,0,1,9,2,96,1,100,112,0,96,0,0,3,3,2,0,1,7,5,1,1,102,0,0,10,7,2,2,0,11,2,0,11]);
    const module = new WebAssembly.Module(bytes);
    assert.throws(function () {
        WebAssembly.Module.exports(module);
    }, TypeError, "WebAssembly.Module.exports unable to produce export descriptors for the given module");
}

{
    // (module
    //     (import "m" "g" (global (ref func)))
    // )
    const bytes = new Uint8Array([0,97,115,109,1,0,0,0,2,9,1,1,109,1,103,3,100,112,0]);
    const module = new WebAssembly.Module(bytes);
    assert.throws(function () {
        WebAssembly.Module.imports(module);
    }, TypeError, "WebAssembly.Module.imports unable to produce import descriptors for the given module");
}

{
    // (module
    //     (import "m" "g" (func (param (ref func))))
    // )
    const bytes = new Uint8Array([0,97,115,109,1,0,0,0,1,6,1,96,1,100,112,0,2,7,1,1,109,1,103,0,0]);
    const module = new WebAssembly.Module(bytes);
    assert.throws(function () {
        WebAssembly.Module.imports(module);
    }, TypeError, "WebAssembly.Module.imports unable to produce import descriptors for the given module");
}
