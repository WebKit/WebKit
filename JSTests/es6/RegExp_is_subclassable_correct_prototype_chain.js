function test() {

class R extends RegExp {}
var r = new R("baz","g");
return r instanceof R && r instanceof RegExp && Object.getPrototypeOf(R) === RegExp;
      
}

if (!test())
    throw new Error("Test failed");

