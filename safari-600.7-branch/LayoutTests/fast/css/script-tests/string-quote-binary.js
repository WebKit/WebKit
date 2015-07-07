description(
'This test checks if CSS string values are correctly serialized when they contain binary characters.'
);

var inputs = ["'\\0\\1\\2\\3\\4\\5\\6\\7\\8\\9\\a\\b\\c\\d\\e\\f'",
              "'\\10\\11\\12\\13\\14\\15\\16\\17\\18\\19\\1a\\1b\\1c\\1d\\1e\\1f\\7f'",
              "'\\A\\B\\C\\D\\E\\F\\1A\\1B\\1C\\1D\\1E\\1F\\7F'",
              "'\\0 \\1 \\2 '",
              "'\\0  \\1  \\2  '",
              "'\\0   \\1   \\2   '",
              "'\\00000f\\00000g'",
              "'\\0 0\\0 1\\0 2\\0 3\\0 4\\0 5\\0 6\\0 7\\0 8\\0 9'",
              "'\\0 A\\0 B\\0 C\\0 D\\0 E\\0 F\\0 G'",
              "'\\0 a\\0 b\\0 c\\0 d\\0 e\\0 f\\0 g'"];
var expected = ["'\\0\\1\\2\\3\\4\\5\\6\\7\\8\\9\\a\\b\\c\\d\\e\\f'",
                "'\\10\\11\\12\\13\\14\\15\\16\\17\\18\\19\\1a\\1b\\1c\\1d\\1e\\1f\\7f'",
                "'\\a\\b\\c\\d\\e\\f\\1a\\1b\\1c\\1d\\1e\\1f\\7f'",
                "'\\0\\1\\2'",          // No space after each control character.
                "'\\0  \\1  \\2  '",    // One space delimiter (that will be ignored by the CSS parser), plus one actual space.
                "'\\0   \\1   \\2   '", // One space delimiter, plus two actual spaces.
                "'\\f\\0g'",
                "'\\0 0\\0 1\\0 2\\0 3\\0 4\\0 5\\0 6\\0 7\\0 8\\0 9'", // Need a space before [0-9A-Fa-f], but not before [Gg].
                "'\\0 A\\0 B\\0 C\\0 D\\0 E\\0 F\\0G'",
                "'\\0 a\\0 b\\0 c\\0 d\\0 e\\0 f\\0g'"];

var testElement = document.createElement('div');
for (var i = 0; i < inputs.length; ++i) {
    testElement.style.fontFamily = inputs[i];
    shouldBeEqualToString('testElement.style.fontFamily', expected[i]);
}
