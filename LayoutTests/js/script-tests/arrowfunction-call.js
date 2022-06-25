description('Tests for ES6 arrow function, this should be overwritten during invoke call&apply');

var d = {
  x : "foo",
  y : function() { return () => this.x; }
};

var e = { x : "bar" };

shouldBe('d.y().call(e)', "'foo'");
shouldBe('d.y().apply(e)', "'foo'");

var successfullyParsed = true;
