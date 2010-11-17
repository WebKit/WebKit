description(
'Tests some regular expressions that used to overflow the regular expression compilation preflight computation.'
);

shouldBe('/(\\d)(\\1{1})/.exec("11").toString()', '"11,1,1"');

shouldBe('/^(\\d{1,2})([ -:\\/\\.]{1})(\\d{1,2})(\\2{1})?(\\d{2,4})?$/.exec("1:1").toString()', '"1:1,1,:,1,,"');

shouldBe('/^(\\d{4})([ -:\\/\\.]{1})(\\d{1,2})(\\2{1})(\\d{1,2})T(\\d{1,2})([ -:\\/\\.]{1})(\\d{1,2})(\\7{1})(\\d{1,2})Z$/.exec("1234:5:6T7/8/9Z").toString()',
    '"1234:5:6T7/8/9Z,1234,:,5,:,6,7,/,8,/,9"');

shouldBe('/\\[["\'\\s]{0,1}([\\w-]*)["\'\\s]{0,1}([\\W]{0,1}=){0,2}["\'\\s]{0,1}([\\w-]*)["\'\\s]{0,1}\\]$/.exec("[]").toString()',
    '"[],,,"');

shouldBe('/(x){0,2}/.exec("").toString()', '","');

shouldBe('/[\u00A1]{4,6}/.exec("\u00A1\u00A1\u00A1\u00A1").toString()', '"\u00A1\u00A1\u00A1\u00A1"');
shouldBe('/[\u00A1]{1,100}[\u00A1]{1,100}[\u00A1]{1,100}[\u00A1]{1,100}[\u00A1]{1,100}[\u00A1]{1,100}[\u00A1]{1,100}[\u00A1]{1,100}/.exec("\u00A1\u00A1\u00A1\u00A1\u00A1\u00A1\u00A1\u00A1").toString()', '"\u00A1\u00A1\u00A1\u00A1\u00A1\u00A1\u00A1\u00A1"');

shouldBe('/{([\\D-\\ca]]„£µ+?)}|[[\\B-\\u00d4]√π- ]]]{0,3}/i.exec("B√π- ]]").toString()', '"B√π- ]],"');
shouldBe('/|[x\\B-\\u00b5]/i.exec("").toString()', '""');

var complexPattern = "";
for (var i = 0; i < 18; ++i)
    complexPattern += "a?";
for (var i = 0; i < 18; ++i)
    complexPattern += "a";
complexPattern = "(" + complexPattern + ")";

var complexInput = "";
for (var i = 0; i < 18; ++i)
    complexInput += "a";

shouldBe('new RegExp(complexPattern).exec(complexInput)[0]', 'complexInput'); // Big but OK
// The analogous "too big" case is tested separately in
// regexp-overflow-too-big.html so engines that don't limit regexp execution
// time don't time out in this test. See
// https://bugs.webkit.org/show_bug.cgi?id=18327 .

var s = "a";
for (var i = 0; i < 21; i++)
    s += s;

shouldThrow('new RegExp(s);');

shouldThrow('/(([ab]){30}){3360}/');
shouldThrow('/(([ab]){30}){0,3360}/');
shouldThrow('/(([ab]){30}){10,3360}/');
shouldThrow('/(([ab]){0,30}){3360}/');
shouldThrow('/(([ab]){0,30}){0,3360}/');
shouldThrow('/(([ab]){0,30}){10,3360}/');
shouldThrow('/(([ab]){10,30}){3360}/');
shouldThrow('/(([ab]){10,30}){0,3360}/');
shouldThrow('/(([ab]){10,30}){10,3360}/');
shouldThrow('/(([ab]){12})(([ab]){65535}){1680}(([ab]){38}){722}([ab]){27}/');

debug('');

var successfullyParsed = true;
