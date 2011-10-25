description(
"This test checks that a multiline comment containing a newline is converted to a line terminator token."
);

var shouldBeUndefined = (function(){
  return/*
  */1
})();

shouldBe('shouldBeUndefined', 'undefined');
