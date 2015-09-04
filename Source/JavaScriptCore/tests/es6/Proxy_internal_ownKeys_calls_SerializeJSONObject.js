function test() {

// SerializeJSONObject -> EnumerableOwnNames -> [[OwnPropertyKeys]]
var ownKeysCalled = 0;
var p = new Proxy({}, { ownKeys: function(o) { ownKeysCalled++; return Object.keys(o); }});
JSON.stringify({a:p,b:p});
return ownKeysCalled === 2;
      
}

if (!test())
    throw new Error("Test failed");

