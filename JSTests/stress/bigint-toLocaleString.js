function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

function shouldNotThrow(func) {
  func();
}

function shouldThrow(func, errorType) {
    let error;
    try {
        func();
    } catch (e) {
        error = e;
    }

    if (!(error instanceof errorType))
        throw new Error(`Expected ${errorType.name}!`);
}

shouldBe(BigInt.prototype.toLocaleString.length, 0);
shouldBe(Object.getOwnPropertyDescriptor(BigInt.prototype, 'toLocaleString').enumerable, false);
shouldBe(Object.getOwnPropertyDescriptor(BigInt.prototype, 'toLocaleString').configurable, true);
shouldBe(Object.getOwnPropertyDescriptor(BigInt.prototype, 'toLocaleString').writable, true);

// Test thisBigIntValue abrupt completion.
shouldNotThrow(() => BigInt.prototype.toLocaleString.call(0n));
shouldNotThrow(() => BigInt.prototype.toLocaleString.call(BigInt(0)));
shouldThrow(() => BigInt.prototype.toLocaleString.call(), TypeError);
shouldThrow(() => BigInt.prototype.toLocaleString.call(undefined), TypeError);
shouldThrow(() => BigInt.prototype.toLocaleString.call(null), TypeError);
shouldThrow(() => BigInt.prototype.toLocaleString.call(1), TypeError);
shouldThrow(() => BigInt.prototype.toLocaleString.call('1'), TypeError);
shouldThrow(() => BigInt.prototype.toLocaleString.call([]), TypeError);
shouldThrow(() => BigInt.prototype.toLocaleString.call(Symbol()), TypeError);

shouldBe(0n.toLocaleString(), '0');
shouldBe(BigInt(1).toLocaleString(), '1');

// Test for NumberFormat behavior.
shouldThrow(() => 0n.toLocaleString('i'), RangeError);

// Test that locale parameter is passed through properly.
shouldBe(123456789n.toLocaleString('ar'), '١٢٣٬٤٥٦٬٧٨٩');
shouldBe(123456789n.toLocaleString('zh-Hans-CN-u-nu-hanidec'), '一二三,四五六,七八九');

// Test that options parameter is passed through properly.
shouldBe(123456n.toLocaleString('en', { maximumSignificantDigits: 3 }), '123,000');
