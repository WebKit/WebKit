function test() {

var buffer = new ArrayBuffer(64);
var constructors = [
  'ArrayBuffer',
  'DataView',
  'Int8Array',
  'Uint8Array',
  'Uint8ClampedArray',
  'Int16Array',
  'Uint16Array',
  'Int32Array',
  'Uint32Array',
  'Float32Array',
  'Float64Array'
];
for(var i = 0; i < constructors.length; i+=1) {
  try {
    if (constructors[i] in global) {
      global[constructors[i]](constructors[i] === "ArrayBuffer" ? 64 : buffer);
    }
    return false;
  } catch(e) {
  }
}
return true;
      
}

if (!test())
    throw new Error("Test failed");

