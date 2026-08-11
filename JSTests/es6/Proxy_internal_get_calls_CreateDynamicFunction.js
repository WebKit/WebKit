function test() {

// CreateDynamicFunction -> GetPrototypeFromConstructor -> Get -> [[Get]]
var get = [];
var p = new Proxy(Function, { get: function(o, k) { get.push(k); return o[k]; }});
new p;
return get + '' === "prototype";
      
}

if (!test())
    throw new Error("Test failed");

