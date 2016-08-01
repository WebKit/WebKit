function test() {

var buffer = new ArrayBuffer(64);
var view = new Int32Array(buffer);        view[0] = 0x80000000;
return view[0] === -0x80000000;
      
}

if (!test())
    throw new Error("Test failed");

