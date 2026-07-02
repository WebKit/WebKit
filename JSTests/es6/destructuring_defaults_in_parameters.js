function test() {

return (function({a = 1, b = 0, c = 3, x:d = 0, y:e = 5},
    [f = 6, g = 0, h = 8]) {
  return a === 1 && b === 2 && c === 3 && d === 4 &&
    e === 5 && f === 6 && g === 7 && h === 8;
}({b:2, c:undefined, x:4},[, 7, undefined]));
      
}

if (!test())
    throw new Error("Test failed");

