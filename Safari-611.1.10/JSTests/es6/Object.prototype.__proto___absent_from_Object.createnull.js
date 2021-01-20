function test() {

var o = Object.create(null), p = {};
o.__proto__ = p;
return Object.getPrototypeOf(o) !== p;
      
}

if (!test())
    throw new Error("Test failed");

