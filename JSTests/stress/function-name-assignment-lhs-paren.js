function shouldBe(a, b) {
  if (a !== b)
    throw new Error(`Expected ${b} but got ${a}`);
}

{
  var foo;
  (foo) = function () { };
  shouldBe(foo.name, "");
}
