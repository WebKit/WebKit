function test() {

var buffer = new ArrayBuffer(64);
var view = new DataView(buffer);
view.setInt16(0, 0x8000);
return view.getInt16(0) === -0x8000;
      
}

if (!test())
    throw new Error("Test failed");

