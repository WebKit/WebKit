description('Test for <a href="https://bugs.webkit.org/show_bug.cgi?id=33057">bug 33057</a>: typeof(xhr.responseText) != "string" on Windows.</p>');

var r = new XMLHttpRequest;
r.open("GET", "webcore-string-comparison.html", false);
r.send();

shouldBe('typeof(r.responseText) == "string"', 'true');
shouldBe('typeof(r.responseText) === "string"', 'true');
shouldBe('typeof(r.responseText) != "string"', 'false');
shouldBe('typeof(r.responseText) !== "string"', 'false');
shouldBe('"" + typeof(r.responseText)', '"string"');
shouldBe('typeof(r.responseText) + ""', '"string"');
shouldBe('String(typeof(r.responseText))', '"string"');
shouldBe('String(typeof(r.responseText))', 'typeof(r.responseText)');
