function test() {

// InstanceofOperator -> GetMethod -> GetV -> [[Get]]
// InstanceofOperator -> OrdinaryHasInstance -> Get -> [[Get]]
var get = [];
var p = new Proxy(Function(), { get: function(o, k) { get.push(k); return o[k]; }});
({}) instanceof p;
return get[0] === Symbol.hasInstance && get.slice(1) + '' === "prototype";
      
}

if (!test())
    throw new Error("Test failed");

