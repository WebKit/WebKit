var b = new Boolean();
b.x = 11;

with (b) {
  f = function(a) { return a*x; } // remember scope chain
}

shouldBe("f(2)", "22");

var OBJECT = new MyObject( "hello" );
function MyObject(value) {
    this.value = value;
    this.toString = new Function( "return this.value+''" );
    return this;
}
shouldBe("OBJECT.toString()", "'hello'");
var s;
with (OBJECT) {
    s = toString();
}
shouldBe("s", "'hello'");


// Make sure that for ... in reevaluates the scoping every time!
P = { foo : 1, bar : 2, baz : 3 }

function testForIn() {
   for (g in P) {
        eval("var g;") //Change the scope of g half-ways through the loop
   }
}

testForIn();
shouldBe("g", "'foo'"); //Before the eval, g was in outer scope, but not after!

var successfullyParsed = true;
