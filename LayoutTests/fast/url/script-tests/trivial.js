description("Test basic features of URL canonicalization");

function canonicalize(url) {
  var a = document.createElement("a");
  a.href = url;
  return a.href;
}

shouldBe("canonicalize('http://example.com/')", "'http://example.com/'");
shouldBe("canonicalize('/')", "'file:///'");

var successfullyParsed = true;
