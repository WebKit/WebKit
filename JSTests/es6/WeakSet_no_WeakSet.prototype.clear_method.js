function test() {

if (!("clear" in WeakSet.prototype)) {
  return true;
}
var s = new WeakSet();
var key = {};
s.add(key);
s.clear();
return s.has(key);
      
}

if (!test())
    throw new Error("Test failed");

