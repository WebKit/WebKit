description(
"This test checks the SIMD-optimized HTML tag pattern for correctness"
);

var htmlTagPattern = /<script|<style|<link/i;
var htmlTagPatternCaseSensitive = /<script|<style|<link/;

shouldBeTrue('htmlTagPattern.test("<script>alert(1)</script>")');
shouldBeTrue('htmlTagPattern.test("<SCRIPT>code</SCRIPT>")');
shouldBeTrue('htmlTagPattern.test("<style>css</style>")');
shouldBeTrue('htmlTagPattern.test("<STYLE>CSS</STYLE>")');
shouldBeTrue('htmlTagPattern.test("<link rel=\\"stylesheet\\">")');
shouldBeTrue('htmlTagPattern.test("<LINK href=\\"file\\">")');
shouldBeTrue('htmlTagPattern.test("<sCrIpT>mixed case</sCrIpT>")');
shouldBeFalse('htmlTagPattern.test("Just plain text")');
shouldBeFalse('htmlTagPattern.test("<div><span>")');
shouldBeFalse('htmlTagPattern.test("script without bracket")');
shouldBeFalse('htmlTagPattern.test("")');

shouldBeTrue('htmlTagPatternCaseSensitive.test("<script>code</script>")');
shouldBeTrue('htmlTagPatternCaseSensitive.test("<style>css</style>")');
shouldBeTrue('htmlTagPatternCaseSensitive.test("<link>")');
shouldBeFalse('htmlTagPatternCaseSensitive.test("<SCRIPT>")');
shouldBeFalse('htmlTagPatternCaseSensitive.test("<STYLE>")');
shouldBeFalse('htmlTagPatternCaseSensitive.test("<LINK>")');

shouldBeTrue('htmlTagPattern.test("<script>at start")');
shouldBeTrue('htmlTagPattern.test("at end<script>")');
shouldBeTrue('htmlTagPattern.test("in<script>middle")');

var longStringWithTag = 'x'.repeat(1000) + '<script>' + 'y'.repeat(1000);
shouldBeTrue('htmlTagPattern.test(longStringWithTag)');
var longStringNoMatch = 'a'.repeat(10000);
shouldBeFalse('htmlTagPattern.test(longStringNoMatch)');

var pattern8Tags = /<div|<span|<p|<a|<img|<br|<ul|<li/i;
shouldBeTrue('pattern8Tags.test("<div>")');
shouldBeTrue('pattern8Tags.test("<li>")');
shouldBeFalse('pattern8Tags.test("<h1>")');

var patternAlphanumeric = /<h1|<h2test|<div3/i;
shouldBeTrue('patternAlphanumeric.test("<h2test>")');
shouldBeFalse('patternAlphanumeric.test("<h2>")');

var patternLongTag = /<verylongtagnameover16chars|<script/i;
shouldBeTrue('patternLongTag.test("<script>")');
shouldBeTrue('patternLongTag.test("<verylongtagnameover16chars>")');
shouldBeFalse('patternLongTag.test("<style>")');
