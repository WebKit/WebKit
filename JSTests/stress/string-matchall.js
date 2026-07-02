function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

function shouldNotThrow(func) {
  func();
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

shouldThrowTypeError(() => { 'abaca'.matchAll(/a/); });
shouldThrowTypeError(() => { 'abaca'.matchAll(new RegExp('a')); });
shouldThrowTypeError(() => { 'abaca'.matchAll({ [Symbol.match]() {} }); });

shouldNotThrow(() => { 'abaca'.matchAll({ [Symbol.match]() {}, flags: 'g' }); });

shouldBe([...'abaca'.matchAll(/a/g)].join(), 'a,a,a');
shouldBe([...'abaca'.matchAll(new RegExp('a', 'g'))].join(), 'a,a,a');
shouldBe([...'abaca'.matchAll({ [Symbol.matchAll]: RegExp.prototype[Symbol.matchAll].bind(/a/g) })].join(), 'a,a,a');
