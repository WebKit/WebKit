function test() {

return typeof Int8Array.prototype.reduceRight === "function" &&
  typeof Uint8Array.prototype.reduceRight === "function" &&
  typeof Uint8ClampedArray.prototype.reduceRight === "function" &&
  typeof Int16Array.prototype.reduceRight === "function" &&
  typeof Uint16Array.prototype.reduceRight === "function" &&
  typeof Int32Array.prototype.reduceRight === "function" &&
  typeof Uint32Array.prototype.reduceRight === "function" &&
  typeof Float32Array.prototype.reduceRight === "function" &&
  typeof Float64Array.prototype.reduceRight === "function";

}

if (!test())
    throw new Error("Test failed");

