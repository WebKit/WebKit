function test() {

return typeof Int8Array.prototype.slice === "function" &&
  typeof Uint8Array.prototype.slice === "function" &&
  typeof Uint8ClampedArray.prototype.slice === "function" &&
  typeof Int16Array.prototype.slice === "function" &&
  typeof Uint16Array.prototype.slice === "function" &&
  typeof Int32Array.prototype.slice === "function" &&
  typeof Uint32Array.prototype.slice === "function" &&
  typeof Float32Array.prototype.slice === "function" &&
  typeof Float64Array.prototype.slice === "function";

}

if (!test())
    throw new Error("Test failed");

