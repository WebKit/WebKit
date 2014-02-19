description(
'This test case tests the Date constructor. ' +
'In particular, it tests many cases of creating a Date from another Date ' +
'and creating a Date from an object that has both valueOf and toString functions.'
);

var object = new Object;
object.valueOf = function() { return 1111; }
object.toString = function() { return "2222"; }

shouldBe('isNaN(new Date(""))', 'true');

var timeZoneOffset = Date.parse("Dec 25 1995") - Date.parse("Dec 25 1995 GMT");

shouldBe('new Date(1111).getTime()', '1111');
shouldBe('new Date(object).getTime()', '1111');
shouldBe('new Date(new Date(1111)).getTime()', '1111');
shouldBe('new Date(new Date(1111).toString()).getTime()', '1000');

shouldBe('new Date(1111, 1).getTime() - timeZoneOffset', '-27104803200000');
shouldBe('new Date(1111, 1, 1).getTime() - timeZoneOffset', '-27104803200000');
shouldBe('new Date(1111, 1, 1, 1).getTime() - timeZoneOffset', '-27104799600000');
shouldBe('new Date(1111, 1, 1, 1, 1).getTime() - timeZoneOffset', '-27104799540000');
shouldBe('new Date(1111, 1, 1, 1, 1, 1).getTime() - timeZoneOffset', '-27104799539000');
shouldBe('new Date(1111, 1, 1, 1, 1, 1, 1).getTime() - timeZoneOffset', '-27104799538999');
shouldBe('new Date(1111, 1, 1, 1, 1, 1, 1, 1).getTime() - timeZoneOffset', '-27104799538999');
shouldBe('new Date(1111, 1, 1, 1, 1, 1, 1, 1, 1).getTime() - timeZoneOffset', '-27104799538999');
shouldBe('new Date(1111, 1, 1, 1, 1, 1, 1, 1, 1).getTime() - timeZoneOffset', '-27104799538999');

shouldBe('new Date(new Date(1111, 1)).getTime() - timeZoneOffset', '-27104803200000');
shouldBe('new Date(new Date(1111, 1, 1)).getTime() - timeZoneOffset', '-27104803200000');
shouldBe('new Date(new Date(1111, 1, 1, 1)).getTime() - timeZoneOffset', '-27104799600000');
shouldBe('new Date(new Date(1111, 1, 1, 1, 1, 1)).getTime() - timeZoneOffset', '-27104799539000');
shouldBe('new Date(new Date(1111, 1, 1, 1, 1, 1, 1)).getTime() - timeZoneOffset', '-27104799538999');
shouldBe('new Date(new Date(1111, 1, 1, 1, 1, 1, 1, 1)).getTime() - timeZoneOffset', '-27104799538999');
shouldBe('new Date(new Date(1111, 1, 1, 1, 1, 1, 1, 1, 1)).getTime() - timeZoneOffset', '-27104799538999');

shouldBe("Number(new Date(new Date(Infinity, 1, 1, 1, 1, 1, 1, 1, 1)).getTime() - timeZoneOffset)", 'Number.NaN');
shouldBe("Number(new Date(new Date(1, Infinity, 1, 1, 1, 1, 1, 1, 1)).getTime() - timeZoneOffset)", 'Number.NaN');
shouldBe("Number(new Date(new Date(1, 1, Infinity, 1, 1, 1, 1, 1, 1)).getTime() - timeZoneOffset)", 'Number.NaN');
shouldBe("Number(new Date(new Date(1, 1, 1, Infinity, 1, 1, 1, 1, 1)).getTime() - timeZoneOffset)", 'Number.NaN');
shouldBe("Number(new Date(new Date(1, 1, 1, 1, Infinity, 1, 1, 1, 1)).getTime() - timeZoneOffset)", 'Number.NaN');
shouldBe("Number(new Date(new Date(1, 1, 1, 1, 1, Infinity, 1, 1, 1)).getTime() - timeZoneOffset)", 'Number.NaN');
shouldBe("Number(new Date(new Date(1, 1, 1, 1, 1, 1, Infinity, 1, 1)).getTime() - timeZoneOffset)", 'Number.NaN');
shouldBe("Number(new Date(new Date(1, 1, 1, 1, 1, 1, 1, 1, Infinity)).getTime() - timeZoneOffset)", '-2174770738999');

shouldBe('new Date(6501480442020679337816440, 81696082856817131586190070, 1, 1, 1, 1, 1).getTime()', 'Number.NaN');

// In Firefox, the results of the following tests are timezone-dependent, which likely implies that the implementation is not quite correct.
// Our results are even worse, though, as the dates are clipped: (new Date(1111, 1201).getTime()) == (new Date(1111, 601).getTime())
// shouldBe('new Date(1111, 1111, 1111, 1111, 1111, 1111, 1111, 1111).getTime() - timeZoneOffset', '-24085894227889');
// shouldBe('new Date(new Date(1111, 1111, 1111, 1111, 1111, 1111, 1111, 1111)).getTime() - timeZoneOffset', '-24085894227889');

// Regression test for Bug 26978 (https://bugs.webkit.org/show_bug.cgi?id=26978)
var testStr = "";
var year = { valueOf: function() { testStr += 1; return 2007; } };
var month = { valueOf: function() { testStr += 2; return 2; } };
var date = { valueOf: function() { testStr += 3; return 4; } };
var hours = { valueOf: function() { testStr += 4; return 13; } };
var minutes = { valueOf: function() { testStr += 5; return 50; } };
var seconds = { valueOf: function() { testStr += 6; return 0; } };
var ms = { valueOf: function() { testStr += 7; return 999; } };

testStr = "";
new Date(year, month, date, hours, minutes, seconds, ms);
shouldBe('testStr', '\"1234567\"');

testStr = "";
Date.UTC(year, month, date, hours, minutes, seconds, ms);
shouldBe('testStr', '\"1234567\"');
