function test() {

var proxied = {};
var passed = false;
Object.keys(
  new Proxy(proxied, {
    ownKeys: function (t) {
      passed = t === proxied; return [];
    }
  })
);
return passed;
      
}

if (!test())
    throw new Error("Test failed");

