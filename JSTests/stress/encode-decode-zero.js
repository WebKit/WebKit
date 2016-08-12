function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

shouldBe(encodeURIComponent('\0'), `%00`);
shouldBe(encodeURI('\0'), `%00`);
shouldBe(escape('\0'), `%00`);

shouldBe(decodeURIComponent('%00'), `\0`);
shouldBe(decodeURI('%00'), `\0`);
shouldBe(unescape('%00'), `\0`);

shouldBe(encodeURIComponent('%00'), `%2500`);
shouldBe(encodeURI('%00'), `%2500`);
shouldBe(escape('%00'), `%2500`);

shouldBe(decodeURIComponent('%2500'), `%00`);
shouldBe(decodeURI('%2500'), `%00`);
shouldBe(unescape('%2500'), `%00`);
