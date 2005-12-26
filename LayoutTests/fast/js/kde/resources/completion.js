var i = 1;

function foo() {
  i = 2;
  return;
  i = 3;
}

shouldBe("foo(), i", "2");

var caught = false;
try { eval("return;"); } catch(e) { caught = true; }
shouldBeTrue("caught");

successfullyParsed = true
