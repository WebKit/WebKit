description(
'Test for https://bugs.webkit.org/show_bug.cgi?id=46077'
);

var re = /^b|^cd/;
var str = "abcd";
shouldBe('re.test(str)', 'false');

var successfullyParsed = true;
