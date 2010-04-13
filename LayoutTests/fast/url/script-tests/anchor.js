description("Test URLs that have an anchor.");

cases = [ 
  ["hello, world", "hello, world"],
  ["\xc2\xa9", "\xc2\xa9"],
  ["\xF0\x90\x8C\x80ss", "\xF0\x90\x8C\x80ss"],
  ["%41%a", "%41%a"],
  ["\xc2", "\xef\xbf\xbd"],
  ["a\xef\xb7\x90", "a\xef\xbf\xbd"],
  ["asdf#qwer", "asdf#qwer"],
  ["#asdf", "#asdf"],
];

for (var i = 0; i < cases.length; ++i) {
  shouldBe("canonicalize('http://www.example.com/#" + cases[i][0] + "')",
           "'http://www.example.com/#" + cases[i][1] + "'");
}

var successfullyParsed = true;
