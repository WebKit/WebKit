description("Test URLs that have a query string.");

cases = [ 
  ["foo=bar", "foo=bar"],
  ["as?df", "as?df"],
  ["as#df", "as%23df"],
  ["\x02hello\x7f bye", "%02hello%7F%20bye"],
  ["%40%41123", "%40%41123"],
  ["q=\xe4\xbd\xa0\xe5\xa5\xbd", "q=%E4%BD%A0%E5%A5%BD"],
  ["q=\xed\xed", "q=%EF%BF%BD%EF%BF%BD"],
  ["q=<asdf>", "q=%3Casdf%3E"],
  ["q=\"asdf\"", "q=%22asdf%22"],
];

for (var i = 0; i < cases.length; ++i) {
  shouldBe("canonicalize('http://www.example.com/?" + cases[i][0] + "')",
           "'http://www.example.com/?" + cases[i][1] + "'");
}

var successfullyParsed = true;
