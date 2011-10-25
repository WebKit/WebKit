description("Canonicalization of path URLs");

cases = [ 
  ["javascript:", "javascript:"],
  ["JavaScript:Foo", "javascript:Foo"],
  // Disabled because this gets treated as a relative URL.
  // [":\":This /is interesting;?#", ":\":This /is interesting;?#"],
];

for (var i = 0; i < cases.length; ++i) {
  test_vector = cases[i][0];
  expected_result = cases[i][1];
  shouldBe("canonicalize('" + test_vector + "')",
           "'" + expected_result + "'");
}
