description("Test resolution of relative UNIX-like URLs.");

cases = [ 
  // Format: [baseURL, relativeURL, expectedURL],
  // On Unix we fall back to relative behavior since there's nothing else
  // reasonable to do.
  ["http://host/a", "\\\\\\\\Another\\\\path", "http://another/path"],

  // Even on Windows, we don't allow relative drive specs when the base
  // is not file.
  ["http://host/a", "/c:\\\\foo", "http://host/c:/foo"],
  ["http://host/a", "//c:\\\\foo", "http://c/foo"],
];

var originalBaseURL = canonicalize(".");

for (var i = 0; i < cases.length; ++i) {
  baseURL = cases[i][0];
  relativeURL = cases[i][1];
  expectedURL = cases[i][2];
  setBaseURL(baseURL);
  shouldBe("canonicalize('" + relativeURL + "')",
           "'" + expectedURL + "'");
}

setBaseURL(originalBaseURL);
