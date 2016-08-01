function test() {

var a = "ba", b = "QUX";
return `foo bar
${a + "z"} ${b.toLowerCase()}` === "foo bar\nbaz qux";
      
}

if (!test())
    throw new Error("Test failed");

