function test() {

return typeof Int8Array.prototype.every === "function" &&
  typeof Uint8Array.prototype.every === "function" &&
  typeof Uint8ClampedArray.prototype.every === "function" &&
  typeof Int16Array.prototype.every === "function" &&
  typeof Uint16Array.prototype.every === "function" &&
  typeof Int32Array.prototype.every === "function" &&
  typeof Uint32Array.prototype.every === "function" &&
  typeof Float32Array.prototype.every === "function" &&
  typeof Float64Array.prototype.every === "function";

}

if (!test())
    throw new Error("Test failed");

