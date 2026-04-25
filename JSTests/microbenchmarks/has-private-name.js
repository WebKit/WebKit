function assert(b) {
  if (!b) throw new Error;
}

class F {
  #x;
  static isF(obj) {
    return #x in obj;
  }
}
noInline(F.prototype.isF);

function test() {
  assert(F.isF(new F) && !F.isF(F) && !F.isF({}));
}
noInline(test);

for (var i = 0; i < 1e6; ++i)
  test();
