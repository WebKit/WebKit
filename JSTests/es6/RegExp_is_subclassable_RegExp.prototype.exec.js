function test() {

class R extends RegExp {}
var r = new R("baz","g");
return r.exec("foobarbaz")[0] === "baz" && r.lastIndex === 9;
      
}

if (!test())
    throw new Error("Test failed");

