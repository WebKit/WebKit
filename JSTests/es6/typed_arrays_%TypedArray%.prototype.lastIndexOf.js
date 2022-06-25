function test() {

return typeof Int8Array.prototype.lastIndexOf === "function" &&
  typeof Uint8Array.prototype.lastIndexOf === "function" &&
  typeof Uint8ClampedArray.prototype.lastIndexOf === "function" &&
  typeof Int16Array.prototype.lastIndexOf === "function" &&
  typeof Uint16Array.prototype.lastIndexOf === "function" &&
  typeof Int32Array.prototype.lastIndexOf === "function" &&
  typeof Uint32Array.prototype.lastIndexOf === "function" &&
  typeof Float32Array.prototype.lastIndexOf === "function" &&
  typeof Float64Array.prototype.lastIndexOf === "function";

}

if (!test())
    throw new Error("Test failed");

