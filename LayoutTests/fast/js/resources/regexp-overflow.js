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

debug('');

var successfullyParsed = true;
