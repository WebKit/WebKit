function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

let count;
const key1 = { toString() { count++; return 'foo'; } };
const key2 = { toString: null, valueOf() { count++; return 'foo'; } };

function test() {
  count = 0;

  ({ [key1]() { return 'bar'; } });
  shouldBe(count, 1);
  ({ [key1]: function () { return 'bar'; } });
  shouldBe(count, 2);
  ({ [key1]: () => 'bar' });
  shouldBe(count, 3);
  ({ get [key1]() { return 'bar'; } });
  shouldBe(count, 4);
  ({ set [key1](_) {} });
  shouldBe(count, 5);

  ({ [key2]() { return 'bar'; } });
  shouldBe(count, 6);
  ({ [key2]: function () { return 'bar'; } });
  shouldBe(count, 7);
  ({ [key2]: () => 'bar' });
  shouldBe(count, 8);
  ({ get [key2]() { return 'bar'; } });
  shouldBe(count, 9);
  ({ set [key2](_) {} });
  shouldBe(count, 10);
}
noInline(test);

for (let i = 0; i < 1e5; i++)
  test();
