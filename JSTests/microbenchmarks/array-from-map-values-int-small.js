const map = new Map([['a', 1], ['b', 2], ['c', 3], ['d', 4], ['e', 5], ['f', 6], ['g', 7]]);

function test(map) {
  const arr = Array.from(map.values());
  return arr;
}
noInline(test);

for (let i = 0; i < 1e4; i++) {
  test(map);
}
