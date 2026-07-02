const set = new Set();
for (let i = 0; i < 1000; i++) {
  set.add(i + 0.5);
}

function test(set) {
  const arr = Array.from(set);
  return arr;
}
noInline(test);

for (let i = 0; i < 1e4; i++) {
  test(set);
}
