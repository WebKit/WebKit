var testCase = function (actual, expected, message) {
  if (actual !== expected) {
    throw message + ". Expected '" + expected + "', but was '" + actual + "'";
  }
};

var d = {
  x : "bar",
  y : function() { return z => this.x + z; }
};

noInline(d.y);

var e = { x : "baz" };

for (var i=0; i<10000; i++) {
  testCase(d.y().bind(e, "ley")(), "barley", "Error: function bind shouldn't change lexical binding of the arrow function");
}
