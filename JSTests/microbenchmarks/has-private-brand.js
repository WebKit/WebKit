function assert(b) {
  if (!b) throw new Error;
}

class M {
  #x() {}
  static isM(obj) {
    return #x in obj;
  }
}
noInline(M.prototype.isM);

function test() {
  assert(M.isM(new M) && !M.isM(M) && !M.isM({}));
}
noInline(test);

for (var i = 0; i < 1e6; ++i)
  test();
