function test() {

var obj = [];
obj.constructor = {};
obj.constructor[Symbol.species] = function() {
    return { foo: 1 };
};
return Array.prototype.splice.call(obj, 0).foo === 1;
      
}

if (!test())
    throw new Error("Test failed");

