description(
'This is a test case for bugs <a href="https://bugs.webkit.org/show_bug.cgi?id=55346">55346</a>, <a href="https://bugs.webkit.org/show_bug.cgi?id=70889">70889</a>, and <a href="https://bugs.webkit.org/show_bug.cgi?id=75452">75452</a>.'
);

shouldBe("({}).toString.call(Error.prototype)", '"[object Error]"');
shouldBe("({}).toString.call(RangeError.prototype)", '"[object Error]"');

var err = new Error("message");
err.name = "";
shouldBe("err.toString()", '"message"');

var err = new Error();
shouldBeFalse("err.hasOwnProperty('message')");

var err = new Error(undefined);
shouldBeFalse("err.hasOwnProperty('message')");
