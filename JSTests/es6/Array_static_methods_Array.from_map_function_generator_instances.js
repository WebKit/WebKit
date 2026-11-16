function test() {

var iterable = (function*(){ yield "foo"; yield "bar"; yield "bal"; }());
return Array.from(iterable, function(e, i) {
  return e + this.baz + i;
}, { baz: "d" }) + '' === "food0,bard1,bald2";
      
}

if (!test())
    throw new Error("Test failed");

