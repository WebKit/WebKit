function test() {
  String.prototype.concat.call({}, "str");
}
noInline(test);

for (let i = 0; i < testLoopCount; i++) {
  test();
}
