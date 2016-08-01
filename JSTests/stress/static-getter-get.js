function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var reg1 = /Cocoa/gi;
shouldBe(reg1.global, true);
shouldBe(reg1.ignoreCase, true);
shouldBe(reg1.multiline, false);
shouldBe(reg1.source, 'Cocoa');

var reg2 = /Cocoa/im;
shouldBe(reg2.global, false);
shouldBe(reg2.ignoreCase, true);
shouldBe(reg2.multiline, true);
shouldBe(reg2.source, 'Cocoa');

var reg3 = /Cappuccino/gm;
shouldBe(reg3.global, true);
shouldBe(reg3.ignoreCase, false);
shouldBe(reg3.multiline, true);
shouldBe(reg3.source, 'Cappuccino');
