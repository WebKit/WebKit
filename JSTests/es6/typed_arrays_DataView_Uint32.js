function test() {

var buffer = new ArrayBuffer(64);
var view = new DataView(buffer);
view.setUint32(0, 0x100000000);
return view.getUint32(0) === 0;
      
}

if (!test())
    throw new Error("Test failed");

