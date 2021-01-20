function test() {

var buffer = new ArrayBuffer(64);
var view = new DataView(buffer);
view.setInt32(0, 0x80000000);
return view.getInt32(0) === -0x80000000;
      
}

if (!test())
    throw new Error("Test failed");

