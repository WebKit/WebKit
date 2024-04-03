function shouldBe(actual, expected) {
  if (actual !== expected)
    throw new Error(`Bad value: ${actual}!`);
}

var xx = 0;
var xxx = 0;

function* genFn() {
  yield "x++";
  yield "throw new Error()";
  yield xx++;
}

var x = 0;

(function() {
  var x = 0;

  for (var i = 0; i < 1e5; i++) {
    eval(...genFn());
    eval(...genFn(), (() => xxx++)());
  }

  shouldBe(x, 1e5 * 2);
  shouldBe(xx, 1e5 * 2);
  shouldBe(xxx, 1e5);
})();

shouldBe(x, 0);
