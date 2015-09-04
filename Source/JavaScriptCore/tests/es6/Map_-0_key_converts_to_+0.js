function test() {

var map = new Map();
map.set(-0, "foo");
var k;
map.forEach(function (value, key) {
  k = 1 / key;
});
return k === Infinity && map.get(+0) == "foo";
      
}

if (!test())
    throw new Error("Test failed");

