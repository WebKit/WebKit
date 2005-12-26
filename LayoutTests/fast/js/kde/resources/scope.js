var b = new Boolean();
b.x = 11;

with (b) {
  f = function(a) { return a*x; } // remember scope chain
}

shouldBe("f(2)", "22");
successfullyParsed = true
