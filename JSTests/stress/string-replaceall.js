function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
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

shouldThrowTypeError(() => { String.prototype.replaceAll.call(undefined, 'def', 'xyz'); });
shouldThrowTypeError(() => { String.prototype.replaceAll.call(null, 'def', 'xyz'); });

shouldThrowTypeError(() => { 'abcdefabcdefabc'.replaceAll(/def/, 'xyz'); });
shouldThrowTypeError(() => { 'abcdefabcdefabc'.replaceAll(new RegExp('def'), 'xyz'); });
shouldThrowTypeError(() => { 'abcdefabcdefabc'.replaceAll({ [Symbol.match]() {}, toString: () => 'def' }, 'xyz'); });

shouldBe('abcdefabcdefabc'.replaceAll('def', 'xyz'), 'abcxyzabcxyzabc');
shouldBe('abcdefabcdefabc'.replaceAll(/def/g, 'xyz'), 'abcxyzabcxyzabc');
shouldBe('abcdefabcdefabc'.replaceAll(new RegExp('def', 'g'), 'xyz'), 'abcxyzabcxyzabc');
shouldBe('abcdefabcdefabc'.replaceAll({ [Symbol.match]() {}, toString: () => 'def', flags: 'g' }, 'xyz'), 'abcxyzabcxyzabc');

const search = /def/g;
search[Symbol.replace] = undefined;
shouldBe('abcdefabcdefabc'.replaceAll(search, 'xyz'), 'abcdefabcdefabc');
search[Symbol.replace] = () => 'q';
shouldBe('abcdefabcdefabc'.replaceAll(search, 'xyz'), 'q');
search[Symbol.replace] = RegExp.prototype[Symbol.replace].bind(search);
shouldBe('abcdefabcdefabc'.replaceAll(search, 'xyz'), 'abcxyzabcxyzabc');

shouldBe('abc'.replaceAll('', 'z'), 'zazbzcz');
shouldBe(''.replaceAll('', 'z'), 'z');
shouldBe('abc'.replaceAll('', ''), 'abc');
shouldBe(''.replaceAll('', ''), '');
