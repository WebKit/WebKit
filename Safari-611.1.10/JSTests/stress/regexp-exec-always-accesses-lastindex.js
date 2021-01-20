function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

function shouldThrowTypeError(func) {
    let error;
    try {
        func();
    } catch (e) {
        error = e;
    }

    if (!(error instanceof TypeError))
        throw new Error('Expected TypeError!');
}

function test1() {
  const r = /./;

  let count = 0;
  r.lastIndex = {
    valueOf() {
      count++;
      return 0;
    }
  };

  r.exec('abc');
  shouldBe(count, 1);
}
noInline(test1);

function test2() {
  const r = /./;

  r.lastIndex = {
    valueOf() {
      return Symbol();
    }
  };

  r.exec('abc');
}
noInline(test2);

for (let i = 0; i < 1e5; i++) {
  test1();
  shouldThrowTypeError(test2);
}
