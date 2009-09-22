function testSwitch(v) {
  var result = "";
  switch (v) {
     case 0:
        result += 'a';
     case 1:
	result += 'b';
     case 1:
        result += 'c';
     case 2:
        result += 'd';
        break;
  }
  return result;
}

shouldBe("testSwitch(0)", "'abcd'");
shouldBe("testSwitch(1)", "'bcd'"); // IE agrees, NS disagrees
shouldBe("testSwitch(2)", "'d'");
shouldBe("testSwitch(false)", "''");

function testSwitch2(v) {
  var result = "";
  switch (v) {
     case 1:
        result += 'a';
        break;
     case 2:
	result += 'b';
        break;
     default:
        result += 'c';
     case 3:
        result += 'd';
        break;
  }
  return result;
}

shouldBe("testSwitch2(1)", "'a'");
shouldBe("testSwitch2(2)", "'b'");
shouldBe("testSwitch2(3)", "'d'");
shouldBe("testSwitch2(-1)", "'cd'");
shouldBe("testSwitch2('x')", "'cd'");

function testSwitch3(v) {
  var result = "";
  switch (v) {
     default:
        result += 'c';
     case 3:
        result += 'd';
     case 4:
        result += 'e';
        break;
  }
  return result;
};

shouldBe("testSwitch3(0)", "'cde'");
shouldBe("testSwitch3(3)", "'de'");
shouldBe("testSwitch3(4)", "'e'");

function testSwitch4(v) {
  var result = "";
  switch (v) {
     case 0:
        result += 'a';
        result += 'b';
        break;
  }
  return result;
};

shouldBe("testSwitch4(0)", "'ab'");

successfullyParsed = true
