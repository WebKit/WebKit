function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function fn() {
  var ok = {};
  var x = function() {
    ok.foo = 42;
  };

  var result = (0, x)(x = 1);

  shouldBe(x, 1);
  shouldBe(ok.foo, 42);
}

fn();
