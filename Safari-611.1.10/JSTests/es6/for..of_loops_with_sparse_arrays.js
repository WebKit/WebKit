function test() {

var arr = [,,];
var count = 0;
for (var item of arr)
  count += (item === undefined);
return count === 2;
      
}

if (!test())
    throw new Error("Test failed");

