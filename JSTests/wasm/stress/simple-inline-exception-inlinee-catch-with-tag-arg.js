var wasm_code = read('simple-inline-exception-inlinee-catch-with-tag-arg.wasm', 'binary')
var wasm_module = new WebAssembly.Module(wasm_code);
var wasm_instance = new WebAssembly.Instance(wasm_module, { m: { ident: (x) => x } });
var f = wasm_instance.exports.main;
try {
    for (let i = 0; i < 1000; ++i)
        f()
} catch (e) {
    print("Caught: ", e.getArg(wasm_instance.exports.tag, 0))
    throw e
}