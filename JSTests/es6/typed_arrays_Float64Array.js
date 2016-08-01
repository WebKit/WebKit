function test() {

var buffer = new ArrayBuffer(64);
var view = new Float64Array(buffer);       view[0] = 0.1;
return view[0] === 0.1;
      
}

if (!test())
    throw new Error("Test failed");

