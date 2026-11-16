function test() {

var buffer = new ArrayBuffer(64);
var view = new Uint16Array(buffer);       view[0] = 0x10000;
return view[0] === 0;
      
}

if (!test())
    throw new Error("Test failed");

