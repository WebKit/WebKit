function test() {

var buffer = new ArrayBuffer(64);
var view = new DataView(buffer);
view.setUint8(0, 0x100);
return view.getUint8(0) === 0;
      
}

if (!test())
    throw new Error("Test failed");

