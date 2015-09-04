function test() {

var d = { x : "bar", y : function() { return z => this.x + z; }};
var e = { x : "baz" };
return d.y().bind(e, "ley")() === "barley";
      
}

if (!test())
    throw new Error("Test failed");

