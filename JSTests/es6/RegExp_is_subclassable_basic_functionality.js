function test() {

class R extends RegExp {}
var r = new R("baz","g");
return r.global && r.source === "baz";
      
}

if (!test())
    throw new Error("Test failed");

