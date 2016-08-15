function test() {

class C extends Array {}
var c = new C();
var len1 = c.length;
c[2] = 'foo';
var len2 = c.length;
return len1 === 0 && len2 === 3;
      
}

if (!test())
    throw new Error("Test failed");

