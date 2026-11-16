function test() {

var buffer = new ArrayBuffer(64);
var view = new DataView(buffer);
view.setInt8 (0, 0x80);
return view.getInt8(0) === -0x80;
      
}

if (!test())
    throw new Error("Test failed");

