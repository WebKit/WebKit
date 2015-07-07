description(
'Tests: Object.prototype.toLocaleString(). Related bug: <a href="https://bugs.webkit.org/show_bug.cgi?id=3989">3989 JSC doesn\'t implement Object.prototype.toLocaleString()</a>'
);

var o = new Object();
shouldBe("o.toLocaleString()", "o.toString()");
o.toLocaleString = function () { return "Dynamic toLocaleString()"; }
shouldBe("o.toLocaleString()", '"Dynamic toLocaleString()"');

shouldBe("Object.prototype.toLocaleString.call('Hello, world!')", '"Hello, world!"');

var stringPrototypeToString = String.prototype.toString;
String.prototype.toString = (function(){ return "stringPrototypeToString"; });
shouldBe("Object.prototype.toLocaleString.call('Hello, world!')", '"stringPrototypeToString"');
String.prototype.toString = stringPrototypeToString;
