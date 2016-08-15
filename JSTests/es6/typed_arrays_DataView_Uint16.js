function test() {

var buffer = new ArrayBuffer(64);
var view = new DataView(buffer);
view.setUint16(0, 0x10000);
return view.getUint16(0) === 0;
      
}

if (!test())
    throw new Error("Test failed");

