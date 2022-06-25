function test() {

var proxied = {};
var passed = false;
Object.isExtensible(
  new Proxy(proxied, {
    isExtensible: function (t) {
      passed = t === proxied; return true;
    }
  })
);
return passed;
      
}

if (!test())
    throw new Error("Test failed");

