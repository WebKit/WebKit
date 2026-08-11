
var wasm_code = new Uint8Array([
  0, 97, 115, 109,    // Magic number indicating this is a WebAssembly binary.
  1, 0, 0, 0,         // Version 1 of the WebAssembly specification.

  // Type Section
  1,   // Section ID.
  6,   // Section size (6 bytes) for the payload excluding the Section ID and size bytes.
  1,   // Number of types defined.
  96,  // Function type.
  1,   // One parameter.
  127, // Parameter is an i32.
  1,   // One return value.
  127, // Return type is an i32.

  // Function Section
  3, // Section ID.
  2, // Section size.
  1, // Number of functions.
  0, // Type index: Function uses type[0]

  // Export Section
  7,  // Section ID.
  8,  // Section size.
  1,  // Number of exports.
  4,  // String length of the export name.
  109, 97, 105, 110, // Export name is `main`.
  0,  // The export is a function.
  0,  // Index of the exported function (first).

  // Code Section
  10, // Section ID for the Code section.
  18, // Section size (18 bytes).
  1,  // One function body.
  16, // Size of the function body (16 bytes).

  0,      // Local count (no additional locals).
  32, 0,  // local.get 0 <num> (retrieve the first parameter)
  65, 10, // i32.const 10
  74,     // i32.gt_s
  4, 127, // if i32 
  65, 1,  //     i32.const 1
  5,      // else
  65, 0,  //     i32.const 0
  11,     // end (if block)
  11,     // end (function)
  11,     // <-- Not expected!

  // Custom Section ...
  0, 24, 4, 110, 97, 109, 101, 1, 7, 1, 0, 4, 109, 97, 105, 110, 2, 8, 1, 0, 1, 0, 3, 110, 117, 109,
]);

let shouldThrow = false;

try {
  var wasm_module = new WebAssembly.Module(wasm_code);
  var wasm_instance = new WebAssembly.Instance(wasm_module);
  const { 'main': func } = wasm_instance.exports;
  var a = func(1);
} catch (e) {
  shouldThrow = true;
}

if (!shouldThrow)
  throw new Error("bad")
