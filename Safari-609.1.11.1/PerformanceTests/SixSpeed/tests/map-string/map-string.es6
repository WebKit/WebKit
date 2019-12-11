var map = new Map();

for (var i = 0; i < 500; i++) {
  map.set(i + '', i);
}

function fn() {
  return map.get('499') === 499;
}

assertEqual(fn(), true);
test(fn);
