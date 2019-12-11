function test() {

var str = "";
for (var item of "𠮷𠮶")
  str += item + " ";
return str === "𠮷 𠮶 ";
      
}

if (!test())
    throw new Error("Test failed");

