function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

if (Intl.getCanonicalLocales('tl')[0] === 'fil') {
    shouldBe(Intl.getCanonicalLocales('nb')[0], 'nb');
    shouldBe(Intl.getCanonicalLocales('no')[0], $vm.icuVersion() >= 69 ? 'no' : 'nb');
    shouldBe(Intl.getCanonicalLocales('iw')[0], 'he');
    shouldBe(Intl.getCanonicalLocales('prs')[0], 'fa-AF');
    shouldBe(Intl.getCanonicalLocales('swc')[0], 'sw-CD');
    shouldBe(Intl.getCanonicalLocales('tl')[0], 'fil');
}
