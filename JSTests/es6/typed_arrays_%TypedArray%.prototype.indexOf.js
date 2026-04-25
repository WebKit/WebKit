function test() {

return typeof Int8Array.prototype.indexOf === "function" &&
  typeof Uint8Array.prototype.indexOf === "function" &&
  typeof Uint8ClampedArray.prototype.indexOf === "function" &&
  typeof Int16Array.prototype.indexOf === "function" &&
  typeof Uint16Array.prototype.indexOf === "function" &&
  typeof Int32Array.prototype.indexOf === "function" &&
  typeof Uint32Array.prototype.indexOf === "function" &&
  typeof Float32Array.prototype.indexOf === "function" &&
  typeof Float64Array.prototype.indexOf === "function";

}

if (!test())
    throw new Error("Test failed");

