description("Test basic features of URL segmentation");

cases = [ 
  // [URL, [SCHEME, HOST, PORT, PATH, QUERY, REF]]
  ["http://example.com/", ["http:", "example.com", "", "/", "", ""]],
];

var originalBaseURL = canonicalize(".");
setBaseURL("http://example.org/foo/bar");

for (var i = 0; i < cases.length; ++i) {
  shouldBe("segments('" + cases[i][0] + "')",
           "'" + JSON.stringify(cases[i][1]) + "'");
}

setBaseURL(originalBaseURL);
