const map = new Map([[1, 'a'], [2, 'b'], [3, 'c'], [4, 'd'], [5, 'e'], [6, 'f'], [7, 'g']]);

function test(map) {
  const arr = Array.from(map.entries());
  return arr;
}
noInline(test);

for (let i = 0; i < 1e4; i++) {
  test(map);
}
