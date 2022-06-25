function test() {

var o = {};
o.__proto__ = Array.prototype;
return o instanceof Array;
      
}

if (!test())
    throw new Error("Test failed");

