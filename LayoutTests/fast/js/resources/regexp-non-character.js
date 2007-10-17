description(
'Test for regular expressions with non-character values in them, specifically in character classes.'
);

shouldBe('"F".match(/[\\uD7FF]/)', 'null');
shouldBe('"0".match(/[\\uD800]/)', 'null');
shouldBe('"F".match(/[\\uDFFF]/)', 'null');
shouldBe('"E".match(/[\\uE000]/)', 'null');
shouldBe('"y".match(/[\\uFDBF]/)', 'null');
shouldBe('"y".match(/[\\uFDD0]/)', 'null');
shouldBe('"y".match(/[\\uFDEF]/)', 'null');
shouldBe('"y".match(/[\\uFDF0]/)', 'null');
shouldBe('"y".match(/[\\uFEFF]/)', 'null');
shouldBe('"y".match(/[\\uFEFF]/)', 'null');
shouldBe('"y".match(/[\\uFFFE]/)', 'null');
shouldBe('"y".match(/[\\uFFFF]/)', 'null');
shouldBe('"y".match(/[\\u10FFFF]/)', 'null');
shouldBe('"y".match(/[\\u110000]/)', 'null');

var successfullyParsed = true;
