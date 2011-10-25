description(
"This test checks that variable declaration 'var arguments;' doesn't override function's local arguments object."
);

// this test needs to use shouldBe() a bit wrong, since passing the string that
// would evaluate the actual expression (like "typeof arguments" instead of
// "'" + typeof arguments + "'" would use the arguments property that would possibly
// be set by the eval function. We want to test the arguments object that is in
// the current scope

// this tests the overriding of arguments in the top-level funcition block
function argumentsLength() {
  var arguments;
  shouldBe("'" + typeof arguments + "'", "'object'");
  return arguments.length;
}

// this tests the overriding of arguments in a block statement inside function block
function argumentsLengthInnerBlock() {
  for (var i=0; i < 1; ++i) {
    var arguments;
    shouldBe("'" + typeof arguments + "'", "'object'");
  }
  return arguments.length;
}

function argumentsLengthInnerBlock2() {
  var i = 0;
  for (var arguments; i > 1; ++i) {
    shouldBe("'" + typeof arguments + "'", "'object'");
  }
  return arguments.length;
}

// this tests that arguments doesn't get overriden by a variable declaration
// with an initializer in a catch block with a parameter of the same name
function argumentsLengthTryCatch() {
  try {
    throw ["foo"];
  } catch (arguments) {
    var arguments = ["foo", "bar"];
  }

  return arguments.length;
}

// this tests that arguments doesn't get overriden by a variable declaration
// with an initializer in a with block where the parameter has a property of
// same name
function argumentsLengthWith() {
  var object = { 'arguments': ["foo"] };

  with (object) {
    var arguments = ["foo", "bar"];
  }

  return arguments.length;
}

// this tests that arguments can still be overridden
function argumentsLengthOverride() {
  shouldBe("'" + typeof arguments + "'", "'object'");
  var argslen = arguments.length;

  var arguments = [0,1,2,3,5,6,7];

  shouldBe("'" + typeof arguments + "'", "'object'");
  shouldBe("" + arguments.length, "7");
  return argslen;
}

function argumentsLengthOverrideInnerBlock() {
  shouldBe("'" + typeof arguments + "'", "'object'");
  var argslen = arguments.length;

  var i = 0;
  for (var arguments = [0,1,2,3,5,6,7]; i < 1; ++i) {
      // empty
  }

  shouldBe("" + arguments.length, "7");

  return argslen;
}

function argumentsLengthOverrideInnerBlock2() {
  shouldBe("'" + typeof arguments + "'", "'object'");
  var argslen = arguments.length;

  for (var i = 0; i < 1; ++i) {
      var arguments = [0,1,2,3,5,6,7];
      // empty
  }

  shouldBe("" + arguments.length, "7");

  return argslen;
}

function argumentsLengthOverrideInnerBlock3() {
  shouldBe("'" + typeof arguments + "'", "'object'");
  var argslen = arguments.length;
  var arguments;
  for (var i = 0; i < 1; ++i) {
      arguments = [0,1,2,3,5,6,7];
      // empty
  }

  shouldBe("" + arguments.length, "7");

  return argslen;
}

function argumentsTearOff1()
{
    return argumentsTearOff2(2);
}

function argumentsTearOff2(b)
{
    var v = b;
    var w = argumentsTearOff1.arguments;
    argumentsTearOff3(3);
    return v;
}

function argumentsTearOff3(c)
{
    var v = c;
}

shouldBe("argumentsLength()", "0");
shouldBe("argumentsLength(1)", "1");
shouldBe("argumentsLength('a','b')", "2");

shouldBe("argumentsLengthInnerBlock()", "0");
shouldBe("argumentsLengthInnerBlock(1)", "1");
shouldBe("argumentsLengthInnerBlock('a','b')", "2");

shouldBe("argumentsLengthInnerBlock2()", "0");
shouldBe("argumentsLengthInnerBlock2(1)", "1");
shouldBe("argumentsLengthInnerBlock2('a','b')", "2");

shouldBe("argumentsLengthTryCatch()", "0");
shouldBe("argumentsLengthWith()", "0");

shouldBe("argumentsLengthOverride()", "0");
shouldBe("argumentsLengthOverride(1)", "1");
shouldBe("argumentsLengthOverride('a','b')", "2");

shouldBe("argumentsLengthOverrideInnerBlock()", "0");
shouldBe("argumentsLengthOverrideInnerBlock(1)", "1");
shouldBe("argumentsLengthOverrideInnerBlock('a','b')", "2");

shouldBe("argumentsLengthOverrideInnerBlock2()", "0");
shouldBe("argumentsLengthOverrideInnerBlock2(1)", "1");
shouldBe("argumentsLengthOverrideInnerBlock2('a','b')", "2");

shouldBe("argumentsLengthOverrideInnerBlock3()", "0");
shouldBe("argumentsLengthOverrideInnerBlock3(1)", "1");
shouldBe("argumentsLengthOverrideInnerBlock3('a','b')", "2");

shouldBe("argumentsTearOff1()", "2");

// this tests that behaviour should persists for
// the program source elements also
shouldBe("typeof undefined", "'undefined'");
shouldBe("'" + typeof arguments + "'", "'undefined'");

var arguments;
shouldBe("typeof arguments", "'object'");
shouldBe("'" + typeof arguments + "'", "'undefined'");

var arguments = [3,2];
shouldBe("'" + typeof arguments + "'", "'object'");
shouldBe("" + arguments.length, "2");
