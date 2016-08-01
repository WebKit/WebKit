function test() {

var c = (v, w, x, y, z) => "" + v + w + x + y + z;
return (c(6, 5, 4, 3, 2) === "65432");
      
}

if (!test())
    throw new Error("Test failed");

