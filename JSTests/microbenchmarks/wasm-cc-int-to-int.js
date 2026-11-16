//@ runDefaultWasm("--useWasm=1")

var wasm_code;
try {
    wasm_code = read('wasm-cc-int-to-int.wasm', 'binary')
} catch {
    try {
        wasm_code = read('../../JSTests/microbenchmarks/wasm-cc-int-to-int.wasm', 'binary')
    } catch {
        wasm_code = read('JSTests/microbenchmarks/wasm-cc-int-to-int.wasm', 'binary')
    }
}
var wasm_module = new WebAssembly.Module(wasm_code);
var wasm_instance = new WebAssembly.Instance(wasm_module);
const { test, test_with_call, test_with_call_indirect } = wasm_instance.exports

for (let i = 0; i < 100000; ++i) {
    test(5)
    test()
    test(null)
    test({ })
    test({ }, 10)
    test(20.1, 10)
    test(10, 20.1)
    test_with_call(5)
    test_with_call()
    test_with_call(null)
    test_with_call({ })
    test_with_call({ }, 10)
    test_with_call(20.1, 10)
    test_with_call(10, 20.1)
    test_with_call_indirect(5)
    test_with_call_indirect()
    test_with_call_indirect(null)
    test_with_call_indirect({ })
    test_with_call_indirect({ }, 10)
    test_with_call_indirect(20.1, 10)
    test_with_call_indirect(10, 20.1)
}
