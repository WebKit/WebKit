function assert(b) {
  if (!b)
      throw new Error("bad!");
}

/*
  (module
  ;; Define a struct type with one i32 field
  (type $myStruct (struct (field i32)))

  ;; Define an array type with i32 elements
  (type $myArray (array (mut i32)))

  ;; Export a function to create a struct
  (func (export "createStruct") (param i32) (result eqref)
      (struct.new $myStruct (local.get 0)))

  ;; Export a function to retrieve a field from a struct
  (func (export "getStructField") (param (ref null $myStruct)) (result i32)
      (struct.get $myStruct 0 (local.get 0))

  ;; Export a function to create an array with default values (0 for i32)
  (func (export "createArray") (param i32) (result (ref $myArray))
      (array.new_default $myArray (local.get 0)))

  ;; Export a function to set an array element
  (func (export "setArrayElement") (param (ref null $myArray)) (param i32) (param i32)
      (array.set $myArray (local.get 0) (local.get 1) (local.get 2)))

  ;; Export a function to get an array element
  (func (export "getArrayElement") (param (ref null $myArray)) (param i32) (result i32)
      (array.get $myArray (local.get 0) (local.get 1)))
  )
*/
const wasmModuleBytes = new Uint8Array([
  0, 97, 115, 109, 1, 0, 0, 0, 1, 167, 128, 128, 128, 0, 7, 95, 1, 127, 0, 94, 127, 1, 96, 1, 127, 1, 109, 96, 1, 99, 0, 1, 127, 96, 1, 127, 1, 100, 1, 96, 3, 99, 1, 127, 127, 0, 96, 2, 99, 1, 127, 1, 127, 3, 134, 128, 128, 128, 0, 5, 2, 3, 4, 5, 6, 7, 211, 128, 128, 128, 0, 5, 12, 99, 114, 101, 97, 116, 101, 83, 116, 114, 117, 99, 116, 0, 0, 14, 103, 101, 116, 83, 116, 114, 117, 99, 116, 70, 105, 101, 108, 100, 0, 1, 11, 99, 114, 101, 97, 116, 101, 65, 114, 114, 97, 121, 0, 2, 15, 115, 101, 116, 65, 114, 114, 97, 121, 69, 108, 101, 109, 101, 110, 116, 0, 3, 15, 103, 101, 116, 65, 114, 114, 97, 121, 69, 108, 101, 109, 101, 110, 116, 0, 4, 10, 196, 128, 128, 128, 0, 5, 135, 128, 128, 128, 0, 0, 32, 0, 251, 0, 0, 11, 136, 128, 128, 128, 0, 0, 32, 0, 251, 2, 0, 0, 11, 135, 128, 128, 128, 0, 0, 32, 0, 251, 7, 1, 11, 139, 128, 128, 128, 0, 0, 32, 0, 32, 1, 32, 2, 251, 14, 1, 11, 137, 128, 128, 128, 0, 0, 32, 0, 32, 1, 251, 11, 1, 11
]);

// Compile the module
const module = new WebAssembly.Module(wasmModuleBytes);

// Instantiate the module
const instance = new WebAssembly.Instance(module);
const { createStruct, getStructField, createArray, setArrayElement, getArrayElement } = instance.exports;

// Test struct creation
const wasmStruct = createStruct(42);
const fieldValue = getStructField(wasmStruct);
assert(fieldValue == 42);
const structPrototype = Object.getPrototypeOf(wasmStruct);
assert(structPrototype === null);

let shouldThrow = false;
try {
  const newPrototype = { protoKey: "protoValue" };
  Object.setPrototypeOf(wasmStruct, newPrototype);
} catch (error) {
  shouldThrow = true;
}
assert(shouldThrow);

// Create an array with 5 elements
const wasmArray = createArray(5);

// Set values in the array
setArrayElement(wasmArray, 0, 42); // Set index 0 to 42
setArrayElement(wasmArray, 1, 99); // Set index 1 to 99

// Retrieve values from the array
assert(42 == getArrayElement(wasmArray, 0));
assert(99 == getArrayElement(wasmArray, 1));
assert(0 == getArrayElement(wasmArray, 2));

const arrayPrototype = Object.getPrototypeOf(wasmArray);
assert(arrayPrototype === null);

shouldThrow = false
try {
  const newPrototype = { protoKey: "protoValue" };
  Object.setPrototypeOf(wasmArray, newPrototype);
} catch (error) {
  shouldThrow = true;
}
assert(shouldThrow);
