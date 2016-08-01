function test() {

// RegExp.prototype.flags -> Get -> [[Get]]
var get = [];
var p = new Proxy({}, { get: function(o, k) { get.push(k); return o[k]; }});
Object.getOwnPropertyDescriptor(RegExp.prototype, 'flags').get.call(p);
return get + '' === "global,ignoreCase,multiline,unicode,sticky";
      
}

if (!test())
    throw new Error("Test failed");

