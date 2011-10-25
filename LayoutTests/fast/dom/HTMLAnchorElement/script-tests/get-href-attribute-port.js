description('Test getting the port attribute of the URL in HTMLAnchorElement.');

var a = document.createElement('a');

debug("Default port is empty");
shouldBe("a.port", "''");

debug("Unspecified port should return empty string");
a.href = "http://example.com/";
shouldBe("a.port", "''");

debug("Empty port should be empty");
a.href = "http://example.com:/";
shouldBe("a.port", "''");
