description('Test setting the href attribute of an HTMLAnchorElement to a URL with leading and trailing whitespace.');

var a = document.createElement('a');

debug("Set href that starts with a space");
a.href = " https://www.mydomain.com/path/testurl.html?key=value";
shouldBe("a.hostname", "'www.mydomain.com'");

debug("Set href that starts with a newline");
a.href = "\nhttps://www.mydomain.com/path/testurl.html?key=value";
shouldBe("a.hostname", "'www.mydomain.com'");

debug("Set href that starts with a tab");
a.href = "\thttps://www.mydomain.com/path/testurl.html?key=value";
shouldBe("a.hostname", "'www.mydomain.com'");

debug("Set href that starts with a carriage return");
a.href = "\rhttps://www.mydomain.com/path/testurl.html?key=value";
shouldBe("a.hostname", "'www.mydomain.com'");

debug("Set href that starts with a combination of newlines, spaces and tabs");
a.href = "\n \t\r \nhttps://www.mydomain.com/path/testurl.html?key=value";
shouldBe("a.hostname", "'www.mydomain.com'");

debug("Set href that ends with a space");
a.href = "https://www.mydomain.com/path/testurl.html?key=value ";
shouldBe("a.hostname", "'www.mydomain.com'");

debug("Set href that ends with a newline");
a.href = "https://www.mydomain.com/path/testurl.html?key=value\n";
shouldBe("a.hostname", "'www.mydomain.com'");

debug("Set href that ends with a tab");
a.href = "https://www.mydomain.com/path/testurl.html?key=value\t";
shouldBe("a.hostname", "'www.mydomain.com'");

debug("Set href that ends with a carriage return");
a.href = "https://www.mydomain.com/path/testurl.html?key=value\r";
shouldBe("a.hostname", "'www.mydomain.com'");

debug("Set href that ends with a combination of newlines, spaces and tabs");
a.href = "https://www.mydomain.com/path/testurl.html?key=value\n \t\r \n";
shouldBe("a.hostname", "'www.mydomain.com'");

debug("Set href that starts and ends with a combination of newlines, spaces and tabs");
a.href = "\n \t\r \nhttps://www.mydomain.com/path/testurl.html?key=value\n \t\r \n";
shouldBe("a.hostname", "'www.mydomain.com'");

var successfullyParsed = true;
