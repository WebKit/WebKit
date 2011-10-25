description("Test basic features of URL canonicalization");

cases = [ 
  ["http://example.com/", "http://example.com/"],
  ["/", "http://example.org/"]
];

var originalBaseURL = canonicalize(".");
setBaseURL("http://example.org/foo/bar");

for (var i = 0; i < cases.length; ++i) {
  shouldBe("canonicalize('" + cases[i][0] + "')",
           "'" + cases[i][1] + "'");
}

setBaseURL(originalBaseURL);
