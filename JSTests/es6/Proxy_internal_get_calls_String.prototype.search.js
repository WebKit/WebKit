function test() {

// String.prototype.search functions -> Get -> [[Get]]
var get = [];
var proxied = {};
proxied[Symbol.toPrimitive] = Function();
var p = new Proxy(proxied, { get: function(o, k) { get.push(k); return o[k]; }});
"".search(p);
return get[0] === Symbol.search && get[1] === Symbol.toPrimitive && get.length === 2;
      
}

if (!test())
    throw new Error("Test failed");

