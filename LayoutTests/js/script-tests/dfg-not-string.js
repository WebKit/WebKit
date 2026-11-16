function foo(text) {
  return !!text
}

function test() {
  var sum = 0;
  var str = ""
  for (var i=0; i < 10; i++) {
    sum += foo(str)

    if (sum < 5)
      str += "a"
  }
  return sum
}

dfgShouldBe(foo, "test()", "9");
