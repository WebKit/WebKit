function test() {

return typeof Int8Array.prototype.join === "function" &&
  typeof Uint8Array.prototype.join === "function" &&
  typeof Uint8ClampedArray.prototype.join === "function" &&
  typeof Int16Array.prototype.join === "function" &&
  typeof Uint16Array.prototype.join === "function" &&
  typeof Int32Array.prototype.join === "function" &&
  typeof Uint32Array.prototype.join === "function" &&
  typeof Float32Array.prototype.join === "function" &&
  typeof Float64Array.prototype.join === "function";

}

if (!test())
    throw new Error("Test failed");

