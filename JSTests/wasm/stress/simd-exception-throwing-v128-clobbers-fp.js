//@ requireOptions("--useWebAssemblySIMD=1")
//@ skip if !$isSIMDPlatform
var wasm_code = read('./simd-exception-throwing-v128-clobbers-fp.wasm', 'binary')
var wasm_module = new WebAssembly.Module(wasm_code);
var wasm_instance = new WebAssembly.Instance(wasm_module);
var f = wasm_instance.exports.main;
try { f() } catch (e) { }