const set = new Set([
  { a: 1 },
  { a: 2 },
  { a: 3 },
  { a: 4 },
  { a: 5 },
  { a: 6 },
  { a: 7 }
]);

function test(set) {
  const arr = Array.from(set);
  return arr;
}
noInline(test);

for (let i = 0; i < 1e5; i++) {
  test(set);
}
