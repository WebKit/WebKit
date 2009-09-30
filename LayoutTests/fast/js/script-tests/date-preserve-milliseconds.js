description(
'The following test checks if an existing milliseconds value gets preserved if a call to setHours(), setMinutes() or setSeconds() does not specify the milliseconds. See <a href="https://bugs.webkit.org/show_bug.cgi?id=3759">https://bugs.webkit.org/show_bug.cgi?id=3759</a>'
);

var d = new Date(0);
d.setMilliseconds(1);

var oldValue = d.getMilliseconds();

d.setHours(8);
shouldBe("d.getMilliseconds()", oldValue.toString());
d.setHours(8, 30);
shouldBe("d.getMilliseconds()", oldValue.toString());
d.setHours(8, 30, 40);
shouldBe("d.getMilliseconds()", oldValue.toString());
d.setMinutes(45);
shouldBe("d.getMilliseconds()", oldValue.toString());
d.setMinutes(45, 40);
shouldBe("d.getMilliseconds()", oldValue.toString());
d.setSeconds(50);
shouldBe("d.getMilliseconds()", oldValue.toString());


var successfullyParsed = true;
