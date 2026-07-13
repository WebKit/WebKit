//@ requireOptions("--useWasmSIMD=1")
//@ skip if !$isSIMDPlatform
//@ $skipModes << "wasm-no-jit".to_sym
//@ $skipModes << "wasm-no-wasm-jit".to_sym
// FIXME: SIMD requires the JIT because IPInt does not interpret it; unskip once IPInt supports SIMD.
var wasm_code = read('./simd-exception-throwing-v128-clobbers-fp.wasm', 'binary')
var wasm_module = new WebAssembly.Module(wasm_code);
var wasm_instance = new WebAssembly.Instance(wasm_module);
var f = wasm_instance.exports.main;
try { f() } catch (e) { }