function test() {

"use strict";
return (function f(n){
  if (n <= 0) {
    return  "foo";
  }
  return f(n - 1);
}(1e6)) === "foo";
      
}

if (!test())
    throw new Error("Test failed");

