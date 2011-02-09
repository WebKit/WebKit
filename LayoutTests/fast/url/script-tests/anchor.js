description("Test URLs that have an anchor.");

cases = [ 
  ["hello, world", "hello, world"],
  ["\xc2\xa9", "\xc2\xa9"],
  ["\ud800\udf00ss", "\ud800\udf00ss"],
  ["%41%a", "%41%a"],
  ["\\ud800\\u597d", "\\uFFFD\\u597D"],
  ["a\\uFDD0", "a\\uFDD0"],
  ["asdf#qwer", "asdf#qwer"],
  ["#asdf", "#asdf"],
  ["a\\nb\\rc\\td", "abcd"],
];

for (var i = 0; i < cases.length; ++i) {
  shouldBe("canonicalize('http://www.example.com/#" + cases[i][0] + "')",
           "'http://www.example.com/#" + cases[i][1] + "'");
}

var successfullyParsed = true;
