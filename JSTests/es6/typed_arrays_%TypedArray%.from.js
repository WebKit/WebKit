function test() {

return typeof Int8Array.from === "function" &&
  typeof Uint8Array.from === "function" &&
  typeof Uint8ClampedArray.from === "function" &&
  typeof Int16Array.from === "function" &&
  typeof Uint16Array.from === "function" &&
  typeof Int32Array.from === "function" &&
  typeof Uint32Array.from === "function" &&
  typeof Float32Array.from === "function" &&
  typeof Float64Array.from === "function";

}

if (!test())
    throw new Error("Test failed");

