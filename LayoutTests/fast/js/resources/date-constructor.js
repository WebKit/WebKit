description(
'This test case tests the Date constructor. ' +
'In particular, it tests many cases of creating a Date from another Date ' +
'and creating a Date from an object that has both valueOf and toString functions.'
);

var object = new Object;
object.valueOf = function() { return 1111; }
object.toSTring = function() { return "2222"; }

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

// In Firefox, the results of the following tests are timezone-dependent, which likely implies that the implementation is not quite correct.
// Our results are even worse, though, as the dates are clipped: (new Date(1111, 1201).getTime()) == (new Date(1111, 601).getTime())
// shouldBe('new Date(1111, 1111, 1111, 1111, 1111, 1111, 1111, 1111).getTime() - timeZoneOffset', '-24085894227889');
// shouldBe('new Date(new Date(1111, 1111, 1111, 1111, 1111, 1111, 1111, 1111)).getTime() - timeZoneOffset', '-24085894227889');

var successfullyParsed = true;
