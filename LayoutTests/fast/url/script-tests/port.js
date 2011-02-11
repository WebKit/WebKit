description("Test URLs that have a port number.");

cases = [ 
  // Invalid input should be copied w/ failure.
  ["as df", ":as%20df"],
  ["-2", ":-2"],
  // Default port should be omitted.
  ["80", ""],
  ["8080", ":8080"],
  // Empty ports (just a colon) should also be removed
  ["", ""],
];

for (var i = 0; i < cases.length; ++i) {
  shouldBe("canonicalize('http://www.example.com:" + cases[i][0] + "/')",
           "'http://www.example.com" + cases[i][1] + "/'");
}

// Unspecified port should mean always keep the port.
shouldBe("canonicalize('foobar://www.example.com:80/')",
         "'foobar://www.example.com:80/'");

var successfullyParsed = true;
