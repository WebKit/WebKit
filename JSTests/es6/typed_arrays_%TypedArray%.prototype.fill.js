function test() {

return typeof Int8Array.prototype.fill === "function" &&
  typeof Uint8Array.prototype.fill === "function" &&
  typeof Uint8ClampedArray.prototype.fill === "function" &&
  typeof Int16Array.prototype.fill === "function" &&
  typeof Uint16Array.prototype.fill === "function" &&
  typeof Int32Array.prototype.fill === "function" &&
  typeof Uint32Array.prototype.fill === "function" &&
  typeof Float32Array.prototype.fill === "function" &&
  typeof Float64Array.prototype.fill === "function";

}

if (!test())
    throw new Error("Test failed");

