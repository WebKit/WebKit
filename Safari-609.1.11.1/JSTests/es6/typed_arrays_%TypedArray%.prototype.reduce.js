function test() {

return typeof Int8Array.prototype.reduce === "function" &&
  typeof Uint8Array.prototype.reduce === "function" &&
  typeof Uint8ClampedArray.prototype.reduce === "function" &&
  typeof Int16Array.prototype.reduce === "function" &&
  typeof Uint16Array.prototype.reduce === "function" &&
  typeof Int32Array.prototype.reduce === "function" &&
  typeof Uint32Array.prototype.reduce === "function" &&
  typeof Float32Array.prototype.reduce === "function" &&
  typeof Float64Array.prototype.reduce === "function";

}

if (!test())
    throw new Error("Test failed");

