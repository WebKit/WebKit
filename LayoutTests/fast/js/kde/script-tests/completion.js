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

// value completions take precedence
var val = eval("11; { }");
shouldBe("val", "11");
val = eval("12; ;");
shouldBe("val", "12");
val = eval("13; if(false);");
shouldBe("val", "13");
val = eval("14; function f() {}");
shouldBe("val", "14");
val = eval("15; var v = 0");
shouldBe("val", "15");

successfullyParsed = true
