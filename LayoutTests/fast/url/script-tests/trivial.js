description("Test basic features of URL canonicalization");

function canonicalize(url) {
  var a = document.createElement("a");
  a.href = url;
  return a.href;
}

cases = [ 
  ["http://example.com/", "http://example.com/"],
  ["/", "file:///"],
];

for (var i = 0; i < cases.length; ++i) {
  shouldBe("canonicalize('" + cases[i][0] + "')",
           "'" + cases[i][1] + "'");
}

var successfullyParsed = true;
