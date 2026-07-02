function test() {

// String.prototype.split functions -> Get -> [[Get]]
var get = [];
var proxied = {};
proxied[Symbol.toPrimitive] = Function();
var p = new Proxy(proxied, { get: function(o, k) { get.push(k); return o[k]; }});
"".split(p);
return get[0] === Symbol.split && get[1] === Symbol.toPrimitive && get.length === 2;
      
}

if (!test())
    throw new Error("Test failed");

