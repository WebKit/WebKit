var testCase = function (actual, expected, message) {
  if (actual !== expected) {
    throw message + ". Expected '" + expected + "', but was '" + actual + "'";
  }
};

var d = {
  x : "foo",
  y : function() { return () => this.x; }
};
noInline(d.y);

var e = { x : "bar" };

for (var i=0; i<10000;i++){
    testCase(d.y().call(e), "foo", "Error: function call shouln't change the lexical binding of the arrow function");
    testCase(d.y().apply(e), "foo", "Error: function apply shouln't change the lexical binding of the arrow function");
}
