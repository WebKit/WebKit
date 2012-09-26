description("This test checks String.localeCompare().");

shouldBeTrue('"a".localeCompare("aa") < 0');
shouldBeTrue('"a".localeCompare("b") < 0');

shouldBeTrue('"a".localeCompare("a") === 0');
shouldBeTrue('"a\u0308\u0323".localeCompare("a\u0323\u0308") === 0');

shouldBeTrue('"aa".localeCompare("a") > 0');
shouldBeTrue('"b".localeCompare("a") > 0');
