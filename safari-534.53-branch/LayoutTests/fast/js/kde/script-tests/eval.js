shouldBe("eval.length", "1");
shouldBe("eval('this')", "this");

function MyObject() {
  this.x = 99;
}

eval("b = new MyObject();");
var bx = b.x   // rule out side effects of eval() in shouldBe() test function
shouldBe("bx", "99");


eval("var c = new MyObject();"); // the 'var' makes a difference
var cx = c.x;
shouldBe("cx", "99");

// KDE bug #45679
if (true.eval) {
  var o = { str:1 };
  shouldBe("o.eval('str')", "1");
  shouldBe("o.eval('this')", "this");
} else {
  testPassed("Skipping test for deprecated Object.prototype.eval()");
}

// problem from within khtml
function lotto() {
  // j must be accessible to eval()
  for (var j = 0; j < 1; j++)
    return eval('j');
}
shouldBe("lotto()", "0");
successfullyParsed = true
