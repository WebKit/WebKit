description(
'Test regular expression processing with alternatives.'
);

var s1 = "<p>content</p>";
shouldBe('s1.match(/<((\\/([^>]+)>)|(([^>]+)>))/)', '["<p>","p>",undefined,undefined,"p>","p"]');
shouldBe('s1.match(/<((ABC>)|(\\/([^>]+)>)|(([^>]+)>))/)', '["<p>","p>",undefined,undefined,undefined,"p>","p"]');
shouldBe('s1.match(/<(a|\\/p|.+?)>/)', '["<p>","p"]');

// Force YARR to use Interpreter by using iterative parentheses
shouldBe('s1.match(/<((\\/([^>]+)>)|((([^>])+)>))/)', '["<p>","p>",undefined,undefined,"p>","p","p"]');
shouldBe('s1.match(/<((ABC>)|(\\/([^>]+)>)|((([^>])+)>))/)', '["<p>","p>",undefined,undefined,undefined,"p>","p","p"]');
shouldBe('s1.match(/<(a|\\/p|(.)+?)>/)', '["<p>","p","p"]');

// Force YARR to use Interpreter by using backreference
var s2 = "<p>p</p>";
shouldBe('s2.match(/<((\\/([^>]+)>)|(([^>]+)>))\\5/)', '["<p>p","p>",undefined,undefined,"p>","p"]');
shouldBe('s2.match(/<((ABC>)|(\\/([^>]+)>)|(([^>]+)>))\\6/)', '["<p>p","p>",undefined,undefined,undefined,"p>","p"]');
shouldBe('s2.match(/<(a|\\/p|.+?)>\\1/)', '["<p>p","p"]');

var successfullyParsed = true;
