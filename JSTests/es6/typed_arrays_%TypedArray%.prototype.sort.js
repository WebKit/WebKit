function test() {

return typeof Int8Array.prototype.sort === "function" &&
  typeof Uint8Array.prototype.sort === "function" &&
  typeof Uint8ClampedArray.prototype.sort === "function" &&
  typeof Int16Array.prototype.sort === "function" &&
  typeof Uint16Array.prototype.sort === "function" &&
  typeof Int32Array.prototype.sort === "function" &&
  typeof Uint32Array.prototype.sort === "function" &&
  typeof Float32Array.prototype.sort === "function" &&
  typeof Float64Array.prototype.sort === "function";

}

if (!test())
    throw new Error("Test failed");

