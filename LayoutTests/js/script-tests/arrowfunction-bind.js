description('Tests for ES6 arrow function, this should be overwritten during invoke bind');

var d = {
  x : "bar",
  y : function() { return z => this.x + z; }
};

var e = { x : "baz" };

shouldBe("d.y().bind(e, 'ley')()", "'barley'");

var successfullyParsed = true;
