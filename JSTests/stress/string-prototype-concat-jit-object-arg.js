function test() {
  "".concat({});
}
noInline(test);

for (let i = 0; i < testLoopCount; i++) {
  test();
}
