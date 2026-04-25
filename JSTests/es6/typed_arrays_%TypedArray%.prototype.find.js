function test() {

return typeof Int8Array.prototype.find === "function" &&
  typeof Uint8Array.prototype.find === "function" &&
  typeof Uint8ClampedArray.prototype.find === "function" &&
  typeof Int16Array.prototype.find === "function" &&
  typeof Uint16Array.prototype.find === "function" &&
  typeof Int32Array.prototype.find === "function" &&
  typeof Uint32Array.prototype.find === "function" &&
  typeof Float32Array.prototype.find === "function" &&
  typeof Float64Array.prototype.find === "function";

}

if (!test())
    throw new Error("Test failed");

