function test() {

var buffer = new ArrayBuffer(64);
var view = new Float32Array(buffer);       view[0] = 0.1;
return view[0] === 0.10000000149011612;
      
}

if (!test())
    throw new Error("Test failed");

