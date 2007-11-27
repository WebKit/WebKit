description(
'Tests some regular expressions with braces in them that used to overflow the regular expression compilation preflight computation.'
);

shouldBe('/(\\d)(\\1{1})/.exec("11").toString()', '"11,1,1"');
shouldBe('/^(\\d{1,2})([ -:\\/\\.]{1})(\\d{1,2})(\\2{1})?(\\d{2,4})?$/.exec("1:1").toString()', '"1:1,1,:,1,,"');
shouldBe('/^(\\d{4})([ -:\\/\\.]{1})(\\d{1,2})(\\2{1})(\\d{1,2})T(\\d{1,2})([ -:\\/\\.]{1})(\\d{1,2})(\\7{1})(\\d{1,2})Z$/.exec("1234:5:6T7/8/9Z").toString()', '"1234:5:6T7/8/9Z,1234,:,5,:,6,7,/,8,/,9"');

debug('');

var successfullyParsed = true;
