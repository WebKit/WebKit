// (module
//  (table (export "t") 0 0 (ref any)
//    (i32.const 0) (ref.i31)
//  )
// )
const wasm1 = new Uint8Array([0, 97, 115, 109, 1, 0, 0, 0, 4, 13, 1, 64, 0, 100, 110, 1, 0, 0, 65, 0, 251, 28, 11, 7, 5, 1, 1, 116, 1, 0]);
const module1 = new WebAssembly.Module(wasm1);
const instance1 = new WebAssembly.Instance(module1);

// (module
//   (import "M" "t" (table 0 0 (ref any)))
// )
const wasm2 = new Uint8Array([0, 97, 115, 109, 1, 0, 0, 0, 2, 11, 1, 1, 77, 1, 116, 1, 100, 110, 1, 0, 0]);
const module2 = new WebAssembly.Module(wasm2);
const instance2 = new WebAssembly.Instance(module2, { M: { t: instance1.exports.t } });
