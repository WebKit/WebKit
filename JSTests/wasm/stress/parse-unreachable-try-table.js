// (module
//   (func (return) (try_table))
// )
const wasm = new Uint8Array([
  0, 97, 115, 109, 1, 0, 0, 0, 1, 4, 1, 96, 0, 0, 3, 2, 1, 0, 10, 9, 1, 7, 0,
  15, 31, 64, 0, 11, 11,
]);
new WebAssembly.Module(wasm);
