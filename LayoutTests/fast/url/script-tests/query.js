description("Test URLs that have a query string.");

cases = [ 
  // Regular ASCII case in some different encodings.
  ["foo=bar", "foo=bar"],
  // Allow question marks in the query without escaping
  ["as?df", "as?df"],
  // Always escape '#' since it would mark the ref.
  // Disabled because this test requires being able to set the query directly.
  // ["as#df", "as%23df"],
  // Escape some questionable 8-bit characters, but never unescape.
  ["\\x02hello\x7f bye", "%02hello%7F%20bye"],
  ["%40%41123", "%40%41123"],
  // Chinese input/output
  ["q=\u4F60\u597D", "q=%26%2320320%3B%26%2322909%3B"],
  // Invalid UTF-8/16 input should be replaced with invalid characters.
  ["q=\\ud800\\ud800", "q=%26%2355296%3B%26%2355296%3B"],
  // Don't allow < or > because sometimes they are used for XSS if the
  // URL is echoed in content. Firefox does this, IE doesn't.
  ["q=<asdf>", "q=%3Casdf%3E"],
  // Escape double quotemarks in the query.
  ["q=\"asdf\"", "q=%22asdf%22"],
];

for (var i = 0; i < cases.length; ++i) {
  shouldBe("canonicalize('http://www.example.com/?" + cases[i][0] + "')",
           "'http://www.example.com/?" + cases[i][1] + "'");
}

var successfullyParsed = true;
