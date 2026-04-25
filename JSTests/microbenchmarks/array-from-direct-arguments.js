function test() {
  const args = Array.from(arguments);
  return args;
}
noInline(test);

for (let i = 0; i < 1e5; i++) {
  test(i, i + 1, i + 2, i + 3, i + 4, i + 5, i + 6);
}
