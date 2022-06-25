function test() {

return new Function("{a = 1, b = 0, c = 3, x:d = 0, y:e = 5}",
  "return a === 1 && b === 2 && c === 3 && d === 4 && e === 5;"
)({b:2, c:undefined, x:4});
      
}

if (!test())
    throw new Error("Test failed");

