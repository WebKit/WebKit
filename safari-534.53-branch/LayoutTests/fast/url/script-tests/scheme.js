description("Canonicalization of URL schemes.");

cases = [ 
  ["http", "http"],
  ["HTTP", "http"],
  // These tests trigger the relative URL resolving behavior of
  // HTMLAnchorElement.href.  In order to test absolute URL parsing, we'd need
  // an API that always maps to absolute URLs.  If you know of one, please
  // enable these tests!
  // [" HTTP ", "%20http%20"],
  // ["htt: ", "htt%3A%20"],
  // ["\xe4\xbd\xa0\xe5\xa5\xbdhttp", "%E4%BD%A0%E5%A5%BDhttp"],
  // ["ht%3Atp", "ht%3atp"],
];

for (var i = 0; i < cases.length; ++i) {
  test_vector = cases[i][0];
  expected_result = cases[i][1];
  shouldBe("canonicalize('" + test_vector + "://example.com/')",
           "'" + expected_result + "://example.com/'");
}
var successfullyParsed = true;
