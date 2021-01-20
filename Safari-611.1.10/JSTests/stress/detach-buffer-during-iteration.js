function test() {
  var ta = new Uint16Array(3);
  try {
    for (var n of ta)
      transferArrayBuffer(ta.buffer);
  } catch {
    return;
  }
  throw 'oh no';
}
noInline(test);

for (var i = 0; i < 1e6; i++)
  test();
