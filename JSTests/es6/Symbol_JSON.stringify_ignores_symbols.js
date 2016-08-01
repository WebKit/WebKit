function test() {

var object = {foo: Symbol()};
object[Symbol()] = 1;
var array = [Symbol()];
return JSON.stringify(object) === '{}' && JSON.stringify(array) === '[null]' && JSON.stringify(Symbol()) === undefined;
      
}

if (!test())
    throw new Error("Test failed");

