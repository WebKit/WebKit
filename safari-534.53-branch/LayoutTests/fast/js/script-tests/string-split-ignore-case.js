description(
'This test checks for a regression against <a href="https://bugs.webkit.org/show_bug.cgi?id=6119">split() function ignores case insensitive modifier</a>.'
);

shouldBe('"1s2S3".split(/s/i).toString()', '"1,2,3"');

var successfullyParsed = true;
