function test(length) {
  const args = Array.from({ length });
  return args;
}
noInline(test);

for (let i = 0; i < 1e5; i++) {
  test(i % 1024);
}
