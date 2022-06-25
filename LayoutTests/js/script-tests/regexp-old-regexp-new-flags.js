description(
'Test for ES6 RegExp construct a new RegExp from exiting RegExp pattern and new flags'
);

var re = new RegExp("Abc");
shouldBeTrue('re.test("   Abc   ")');
shouldBe('re.flags', '""');

re = new RegExp(re, "i");
shouldBeTrue('re.test(" ABC  ")');
shouldBe('re.flags', '"i"');

re = new RegExp(re, "");
shouldBeTrue('re.test("   Abc   ")');
shouldBe('re.flags', '""');

re = new RegExp(re, "iy");
shouldBe('re.exec("abcABCAbc").toString()', '"abc"');
shouldBe('re.exec("abcABCAbc").toString()', '"ABC"');
shouldBe('re.exec("abcABCAbc").toString()', '"Abc"');
shouldBe('re.flags', '"iy"');

re = new RegExp(re, "");
shouldBeFalse('re.test("abc")');

shouldThrow('new RegExp(re, "bad flags")', '"SyntaxError: Invalid flags supplied to RegExp constructor."');
