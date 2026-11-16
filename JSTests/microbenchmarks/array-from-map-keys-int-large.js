const map = new Map();
for (let i = 0; i < 1000; i++) {
  map.set(i, `value${i}`);
}

function test(map) {
  const arr = Array.from(map.keys());
  return arr;
}
noInline(test);

for (let i = 0; i < 1e4; i++) {
  test(map);
}
