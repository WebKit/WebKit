const set = new Set([1, 2, 3, 4, 5, 6, 7]);

function test(set) {
  const arr = Array.from(set);
  return arr;
}
noInline(test);

for (let i = 0; i < 1e5; i++) {
  test(set);
}
