function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function fn() {
  var ok = {};
  var x = function() {
    ok.foo = 42;
  };

  var result = x`_${x = 1}_`

  shouldBe(x, 1);
  shouldBe(ok.foo, 42);
}

fn();
