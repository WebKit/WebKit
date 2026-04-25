function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function fn() {
  var ok = {};
  var x = function() {
    ok.foo = 42;
  };

  var result = new x({ x } = { x: 1 });

  shouldBe(x, 1);
  shouldBe(ok.foo, 42);
}

function fn2() {
  var ok = {};
  var x = function() {
    ok.foo = 42;
  };

  var result = new x([ x ] = [ 1 ]);

  shouldBe(x, 1);
  shouldBe(ok.foo, 42);
}

fn();
fn2();
