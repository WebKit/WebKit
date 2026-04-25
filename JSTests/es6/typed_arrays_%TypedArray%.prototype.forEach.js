function test() {

return typeof Int8Array.prototype.forEach === "function" &&
  typeof Uint8Array.prototype.forEach === "function" &&
  typeof Uint8ClampedArray.prototype.forEach === "function" &&
  typeof Int16Array.prototype.forEach === "function" &&
  typeof Uint16Array.prototype.forEach === "function" &&
  typeof Int32Array.prototype.forEach === "function" &&
  typeof Uint32Array.prototype.forEach === "function" &&
  typeof Float32Array.prototype.forEach === "function" &&
  typeof Float64Array.prototype.forEach === "function";

}

if (!test())
    throw new Error("Test failed");

