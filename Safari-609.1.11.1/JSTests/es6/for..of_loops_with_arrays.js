function test() {

var arr = [5];
for (var item of arr)
  return item === 5;
      
}

if (!test())
    throw new Error("Test failed");

