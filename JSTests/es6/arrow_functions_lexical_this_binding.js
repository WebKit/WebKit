function test() {

var d = { x : "bar", y : function() { return z => this.x + z; }}.y();
var e = { x : "baz", y : d };
return d("ley") === "barley" && e.y("ley") === "barley";
      
}

if (!test())
    throw new Error("Test failed");

