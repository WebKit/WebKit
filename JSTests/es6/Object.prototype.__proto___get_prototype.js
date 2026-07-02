function test() {

var A = function(){};
return (new A()).__proto__ === A.prototype;
      
}

if (!test())
    throw new Error("Test failed");

