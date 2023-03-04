//@skip
// FIXME: rdar://106193899 (delegate can only target try blocks or the top level of the function)
// This was broken even without inlining.
var wasm_code = read('simple-inline-exception-inlinee-catch-with-delegate-block.wasm', 'binary')
var wasm_module = new WebAssembly.Module(wasm_code);
var wasm_instance = new WebAssembly.Instance(wasm_module, { m: { ident: (x) => x } });
var f = wasm_instance.exports.main;
try {
    for (let i = 0; i < 1000; ++i)
        f()
} catch (e) {
    print(e)
    print("Caught: ", e.getArg(wasm_instance.exports.tag, 0))
    throw e
}