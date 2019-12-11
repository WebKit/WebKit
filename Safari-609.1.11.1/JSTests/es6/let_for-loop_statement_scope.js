function test() {

let baz = 1;
for(let baz = 0; false; false) {}
return baz === 1;
      
}

if (!test())
    throw new Error("Test failed");

