function test() {

return new RegExp(/./im, "g").global === true;
      
}

if (!test())
    throw new Error("Test failed");

