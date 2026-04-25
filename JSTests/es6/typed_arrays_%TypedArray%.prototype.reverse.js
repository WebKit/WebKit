function test() {

return typeof Int8Array.prototype.reverse === "function" &&
  typeof Uint8Array.prototype.reverse === "function" &&
  typeof Uint8ClampedArray.prototype.reverse === "function" &&
  typeof Int16Array.prototype.reverse === "function" &&
  typeof Uint16Array.prototype.reverse === "function" &&
  typeof Int32Array.prototype.reverse === "function" &&
  typeof Uint32Array.prototype.reverse === "function" &&
  typeof Float32Array.prototype.reverse === "function" &&
  typeof Float64Array.prototype.reverse === "function";

}

if (!test())
    throw new Error("Test failed");

