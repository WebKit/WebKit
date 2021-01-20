class C extends Array {}
var c = new C();
c.push(2,4,6);
var result = c.slice(9,2);
if (result.length)
    throw ("FAILED: expected 0, actual " + result.length);

