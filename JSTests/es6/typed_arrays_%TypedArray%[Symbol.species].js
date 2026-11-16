function test() {

return typeof Int8Array[Symbol.species] === "function" &&
  typeof Uint8Array[Symbol.species] === "function" &&
  typeof Uint8ClampedArray[Symbol.species] === "function" &&
  typeof Int16Array[Symbol.species] === "function" &&
  typeof Uint16Array[Symbol.species] === "function" &&
  typeof Int32Array[Symbol.species] === "function" &&
  typeof Uint32Array[Symbol.species] === "function" &&
  typeof Float32Array[Symbol.species] === "function" &&
  typeof Float64Array[Symbol.species] === "function";

}

if (!test())
    throw new Error("Test failed");

