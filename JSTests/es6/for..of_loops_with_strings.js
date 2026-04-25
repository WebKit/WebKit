function test() {

var str = "";
for (var item of "foo")
  str += item;
return str === "foo";
      
}

if (!test())
    throw new Error("Test failed");

