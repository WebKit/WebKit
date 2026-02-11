//@ requireOptions("--useB3SparseConditionalConstantPropagation=true", "--useConcurrentJIT=0")

function assert(b) {
  if (!b) throw new Error;
}

function shouldThrowTypeError(func) {
    let error;
    try {
        func();
    } catch (e) {
        error = e;
    }

    if (!(error instanceof TypeError))
        throw new Error('Expected TypeError!');
}

class F { #x; static isF(obj) { return #x in obj; } }
class SF { static #x; static isSF(obj) { return #x in obj; } }
class M { #x() {} static isM(obj) { return #x in obj; } }

function test() {
  assert(F.isF(new F));
  assert(!F.isF(F) && !F.isF(new M) && !F.isF({}));
  shouldThrowTypeError(() => F.isF(3));
}
noInline(test);

for (let i = 0; i < testLoopCount; i++)
  test();
