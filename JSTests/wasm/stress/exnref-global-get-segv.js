// (module
//   (global (export "g") (ref null exn) (ref.null exn))
// )
const wasm1 = new Uint8Array([0, 97, 115, 109, 1, 0, 0, 0, 6, 6, 1, 105, 0, 208, 105, 11, 7, 5, 1, 1, 103, 3, 0]);
const module1 = new WebAssembly.Module(wasm1);
const instance1 = new WebAssembly.Instance(module1);

// (module
//   (import "M" "g" (global (ref null exn)))
// )
const wasm2 = new Uint8Array([0, 97, 115, 109, 1, 0, 0, 0, 2, 8, 1, 1, 77, 1, 103, 3, 105, 0]);
const module2 = new WebAssembly.Module(wasm2);
try {
    const instance2 = new WebAssembly.Instance(module2, { M: { g: instance1.exports.g } });
} catch {
    // no segv
}
