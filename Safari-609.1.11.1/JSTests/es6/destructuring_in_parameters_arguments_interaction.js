function test() {

return (function({a, x:b, y:e}, [c, d]) {
  return arguments[0].a === 1 && arguments[0].x === 2
    && !("y" in arguments[0]) && arguments[1] + '' === "3,4";
}({a:1, x:2}, [3, 4]));
      
}

if (!test())
    throw new Error("Test failed");

