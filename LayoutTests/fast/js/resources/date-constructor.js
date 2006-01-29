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

// I believe these next results may be incorrect.
// Firefox gives different results.
// Keep the tests here as a regression test for now.
shouldBe('new Date(1111, 1111, 1111, 1111, 1111, 1111, 1111, 1111).getTime() - timeZoneOffset', '-28085817599889');
shouldBe('new Date(new Date(1111, 1111, 1111, 1111, 1111, 1111, 1111, 1111)).getTime() - timeZoneOffset', '-28085817599889');

var successfullyParsed = true;
