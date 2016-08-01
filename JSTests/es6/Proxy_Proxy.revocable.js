function test() {

var obj = Proxy.revocable({}, { get: function() { return 5; } });
var passed = (obj.proxy.foo === 5);
obj.revoke();
try {
  obj.proxy.foo;
} catch(e) {
  passed &= e instanceof TypeError;
}
return passed;
      
}

if (!test())
    throw new Error("Test failed");

