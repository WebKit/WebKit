function test() {

var buffer = new ArrayBuffer(64);
var view = new DataView(buffer);
view.setFloat32(0, 0.1);
return view.getFloat32(0) === 0.10000000149011612;
      
}

if (!test())
    throw new Error("Test failed");

