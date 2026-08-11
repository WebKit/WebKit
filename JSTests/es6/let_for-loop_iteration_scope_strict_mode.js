function test() {

'use strict';
let scopes = [];
for(let i = 0; i < 2; i++) {
  scopes.push(function(){ return i; });
}
let passed = (scopes[0]() === 0 && scopes[1]() === 1);

scopes = [];
for(let i in { a:1, b:1 }) {
  scopes.push(function(){ return i; });
}
passed &= (scopes[0]() === "a" && scopes[1]() === "b");
return passed;
      
}

if (!test())
    throw new Error("Test failed");

