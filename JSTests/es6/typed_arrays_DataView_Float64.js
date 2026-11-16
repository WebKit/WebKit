function test() {

var buffer = new ArrayBuffer(64);
var view = new DataView(buffer);
view.setFloat64(0, 0.1);
return view.getFloat64(0) === 0.1;
      
}

if (!test())
    throw new Error("Test failed");

