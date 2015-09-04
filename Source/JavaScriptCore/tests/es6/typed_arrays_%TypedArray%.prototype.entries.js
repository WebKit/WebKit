function test() {

return typeof Int8Array.prototype.entries === "function" &&
  typeof Uint8Array.prototype.entries === "function" &&
  typeof Uint8ClampedArray.prototype.entries === "function" &&
  typeof Int16Array.prototype.entries === "function" &&
  typeof Uint16Array.prototype.entries === "function" &&
  typeof Int32Array.prototype.entries === "function" &&
  typeof Uint32Array.prototype.entries === "function" &&
  typeof Float32Array.prototype.entries === "function" &&
  typeof Float64Array.prototype.entries === "function";

}

if (!test())
    throw new Error("Test failed");

