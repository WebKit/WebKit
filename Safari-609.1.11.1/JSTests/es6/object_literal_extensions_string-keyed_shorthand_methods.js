function test() {

return ({ "foo bar"() { return 4; } })["foo bar"]() === 4;
      
}

if (!test())
    throw new Error("Test failed");

