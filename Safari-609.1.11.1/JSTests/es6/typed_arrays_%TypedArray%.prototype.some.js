function test() {

return typeof Int8Array.prototype.some === "function" &&
  typeof Uint8Array.prototype.some === "function" &&
  typeof Uint8ClampedArray.prototype.some === "function" &&
  typeof Int16Array.prototype.some === "function" &&
  typeof Uint16Array.prototype.some === "function" &&
  typeof Int32Array.prototype.some === "function" &&
  typeof Uint32Array.prototype.some === "function" &&
  typeof Float32Array.prototype.some === "function" &&
  typeof Float64Array.prototype.some === "function";

}

if (!test())
    throw new Error("Test failed");

