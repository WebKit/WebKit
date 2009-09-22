description(
'Tests: Object.prototype.toLocaleString(). Related bug: <a href="http://bugzilla.opendarwin.org/show_bug.cgi?id=3989">3989 JSC doesn\'t implement Object.prototype.toLocaleString()</a>'
);

var o = new Object();
shouldBe("o.toLocaleString()", "o.toString()");
o.toLocaleString = function () { return "Dynamic toLocaleString()"; }
shouldBe("o.toLocaleString()", '"Dynamic toLocaleString()"');

var successfullyParsed = true;
