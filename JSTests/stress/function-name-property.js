function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

shouldBe(({ f: () => {} }).f.name, 'f');
shouldBe(({ f: function () {} }).f.name, 'f');
shouldBe(({ ['f']: () => {} }).f.name, 'f');
shouldBe(({ ['f']: function () {} }).f.name, 'f');
shouldBe(({ async f() {} }).f.name, 'f');
shouldBe(({ async ['f']() {} }).f.name, 'f');
shouldBe((class { f() {} }).prototype.f.name, 'f');
shouldBe((class { ['f']() {} }).prototype.f.name, 'f');
shouldBe((class { async f() {} }).prototype.f.name, 'f');
shouldBe((class { async ['f']() {} }).prototype.f.name, 'f');

shouldBe([() => {}][0].name, '');
shouldBe([function () {}][0].name, '');
shouldBe(({ 0: () => {} })[0].name, '0');
shouldBe(({ 0: function () {} })[0].name, '0');
shouldBe(({ [0]: () => {} })[0].name, '0');
shouldBe(({ [0]: function () {} })[0].name, '0');
shouldBe((class { 0() {} }).prototype[0].name, '0');
shouldBe((class { [0]() {} }).prototype[0].name, '0');
