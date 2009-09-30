description(
'Test for regression against <a href="https://bugs.webkit.org/show_bug.cgi?id=5602">REGRESSION: RegExp("[^\\s$]+", "g") returns extra matches</a>'
);

var re = new RegExp("[^\\s$]+", "g");
var accumulate = "";
var match;
shouldBe('while (match = re.exec("  abcdefg")) accumulate += match + "; "; accumulate', '"abcdefg; "');

var re = new RegExp(/\d/g);
accumulate = "";
shouldBe('while (match = re.exec("123456789")) accumulate += match + "; "; accumulate', '"1; 2; 3; 4; 5; 6; 7; 8; 9; "');

var successfullyParsed = true;
