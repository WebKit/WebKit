//@ requireOptions("--useB3SparseConditionalConstantPropagation=true", "--useConcurrentJIT=0")

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

function test() {
  shouldThrowTypeError(() => F.isF(3));
}
noInline(test);

for (let i = 0; i < testLoopCount; i++)
  test();
