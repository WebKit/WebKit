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

  for (var i = 0; i < testLoopCount; i++) {
    eval(...genFn());
    eval(...genFn(), (() => xxx++)());
  }

  shouldBe(x, testLoopCount * 2);
  shouldBe(xx, testLoopCount * 2);
  shouldBe(xxx, testLoopCount);
})();

shouldBe(x, 0);
