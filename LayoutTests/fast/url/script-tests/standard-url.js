description("Canonicalization of standard URLs");

cases = [ 
  ["http://www.google.com/foo?bar=baz#", "http://www.google.com/foo?bar=baz#"],
  ["http://[www.google.com]/", "http://[www.google.com]/"],
  // Disabled because whitespace gets treated different in this API.
  // ["ht\ttp:@www.google.com:80/;p?#", "ht%09tp://www.google.com:80/;p?#"],
  ["http:////////user:@google.com:99?foo", "http://user@google.com:99/?foo"],
  // Disabled because this gets treated as a relative URL.
  // ["www.google.com", ":www.google.com/"],
  ["http://192.0x00A80001", "http://192.168.0.1/"],
  ["http://www/foo%2Ehtml", "http://www/foo.html"],
  ["http://user:pass@/", "http://user:pass@/"],
  ["http://%25DOMAIN:foobar@foodomain.com/", "http://%25DOMAIN:foobar@foodomain.com/"],
  // Backslashes should get converted to forward slashes.
  ["http:\\\\\\\\www.google.com\\\\foo", "http://www.google.com/foo"],
  // Busted refs shouldn't make the whole thing fail.
  ["http://www.google.com/asdf#\\ud800", "http://www.google.com/asdf#\\uFFFD"],
  // Basic port tests.
  ["http://foo:80/", "http://foo/"],
  ["http://foo:81/", "http://foo:81/"],
  ["httpa://foo:80/", "httpa://foo:80/"],
  ["http://foo:-80/", "http://foo:-80/"],
  ["https://foo:443/", "https://foo/"],
  ["https://foo:80/", "https://foo:80/"],
  ["ftp://foo:21/", "ftp://foo/"],
  ["ftp://foo:80/", "ftp://foo:80/"],
  ["gopher://foo:70/", "gopher://foo/"],
  ["gopher://foo:443/", "gopher://foo:443/"],
  ["ws://foo:80/", "ws://foo/"],
  ["ws://foo:81/", "ws://foo:81/"],
  ["ws://foo:443/", "ws://foo:443/"],
  ["ws://foo:815/", "ws://foo:815/"],
  ["wss://foo:80/", "wss://foo:80/"],
  ["wss://foo:81/", "wss://foo:81/"],
  ["wss://foo:443/", "wss://foo/"],
  ["wss://foo:815/", "wss://foo:815/"],
];

for (var i = 0; i < cases.length; ++i) {
  test_vector = cases[i][0];
  expected_result = cases[i][1];
  shouldBe("canonicalize('" + test_vector + "')",
           "'" + expected_result + "'");
}

var successfullyParsed = true;
