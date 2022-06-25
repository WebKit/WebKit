function test() {

var d = { x : "foo", y : function() { return () => this.x; }};
var e = { x : "bar" };
return d.y().call(e) === "foo" && d.y().apply(e) === "foo";
      
}

if (!test())
    throw new Error("Test failed");

