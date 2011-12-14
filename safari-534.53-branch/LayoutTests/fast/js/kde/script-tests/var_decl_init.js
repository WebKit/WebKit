var myvar = 1;

function varInFunction() {
  return (myvar == undefined);
  var myvar = 2;
}

function varInVarList() {
  return (myvar == undefined);
  var a = 1, b = 0, myvar = 2;
}

function varListOrder() {
  var tmp = 0;
  var i = ++tmp, j = ++tmp;
  return j == 2;
}

function varInBlock() {
  return (myvar == undefined);
  {
    var myvar = 2;
  }
}

function varInIf() {
  return (myvar == undefined);
  if (false)
    var myvar = 2;
}

function varInElse() {
  return (myvar == undefined);
  if (true) {
  }
  else
    var myvar = 2;
}

function varInDoWhile() {
  return (myvar == undefined);
  do
    var myvar = 2;
  while (false);
}

function varInWhile() {
  return (myvar == undefined);
  while (false)
    var myvar = 2;
}

function varInFor() {
  return (myvar == undefined);
  var i;
  for (i = 0; i < 0; i++)
    var myvar = 2;
}

function varInForInitExpr() {
  return (myvar == undefined);
  for (var myvar = 2; i < 2; i++) {
  }
}

function varInForIn() {
  return (myvar == undefined);
  var o = new Object();
  var i;
  for (i in o)
    var myvar = 2;
}

function varInWith() {
  return (myvar == undefined);
  with (String)
    var myvar = 2;
}

function varInCase() {
  return (myvar == undefined);
  switch (1) {
    case 0:
      var myvar = 2;
    case 1:
  }
}

function varInLabel() {
  return (myvar == undefined);
label1:
  var myvar = 2;
}

function varInCatch() {
  return (myvar == undefined);
  try {
  }
  catch (e) {
    var myvar = 2;
  }
}

function varInFinally() {
  return (myvar == undefined);
  try {
  }
  finally {
    var myvar = 2;
  }
}

function varInTry() {
  return (myvar == undefined);
  try {
    var myvar = 2;
  }
  catch (e) {
  }
  finally {
  }
}

function varInSubFunction() {
  return (myvar == 1);
  function subfunction() {
    var myvar = 2;
  }
}

if (!varGlobal)
  var varGlobal = 1;

shouldBe("varInFunction()","true");
shouldBe("varInVarList()","true");
shouldBe("varListOrder()","true");
shouldBe("varInBlock()","true");
shouldBe("varInIf()","true");
shouldBe("varInElse()","true");
shouldBe("varInDoWhile()","true");
shouldBe("varInWhile()","true");
shouldBe("varInFor()","true");
shouldBe("varInForIn()","true");
shouldBe("varInWith()","true");
shouldBe("varInCase()","true");
shouldBe("varInLabel()","true");
shouldBe("varInCatch()","true");
shouldBe("varInFinally()","true");
shouldBe("varInTry()","true");
shouldBe("varInForInitExpr()","true");
shouldBe("varInSubFunction()","true");
shouldBe("varGlobal", "1");

var overrideVar = 1;
var overrideVar;
shouldBe("overrideVar", "1");

var overrideVar2 = 1;
var overrideVar2 = 2;
shouldBe("overrideVar2", "2");
successfullyParsed = true
