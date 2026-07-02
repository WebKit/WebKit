function test() {

var set = new Set();
set.add(-0);
var k;
set.forEach(function (value) {
  k = 1 / value;
});
return k === Infinity && set.has(+0);
      
}

if (!test())
    throw new Error("Test failed");

