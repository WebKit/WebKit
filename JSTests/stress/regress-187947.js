function shouldBe(actual, expected) {
  if (actual !== expected)
    throw new Error('bad value: ' + actual);
}

var ab16bit = 'abcĀ'.replace(/c.*/, '');

var map = {};
map[ab16bit];

var ropeAB = 'a' + 'b';
var ropeABC = ropeAB + 'c';

map[ropeAB];
map[ropeABC] = 42;
shouldBe(JSON.stringify(map), '{"abc":42}');
