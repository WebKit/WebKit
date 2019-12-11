function test() {

// Date.prototype.toJSON -> ToPrimitive -> Get -> [[Get]]
// Date.prototype.toJSON -> Invoke -> GetMethod -> GetV -> [[Get]]
var get = [];
var p = new Proxy({toString:Function(),toISOString:Function()}, { get: function(o, k) { get.push(k); return o[k]; }});
Date.prototype.toJSON.call(p);
return get[0] === Symbol.toPrimitive && get.slice(1) + '' === "valueOf,toString,toISOString";
      
}

if (!test())
    throw new Error("Test failed");

