function test() {

// SetIntegrityLevel -> [[OwnPropertyKeys]]
var ownKeysCalled = 0;
var p = new Proxy({}, { ownKeys: function(o) { ownKeysCalled++; return Object.keys(o); }});
Object.freeze(p);
return ownKeysCalled === 1;
      
}

if (!test())
    throw new Error("Test failed");

