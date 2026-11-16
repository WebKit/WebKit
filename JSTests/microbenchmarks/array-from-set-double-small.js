const set = new Set([1.1, 2.2, 3.3, 4.4, 5.5, 6.6, 7.7]);

function test(set) {
  const arr = Array.from(set);
  return arr;
}
noInline(test);

for (let i = 0; i < 1e5; i++) {
  test(set);
}
