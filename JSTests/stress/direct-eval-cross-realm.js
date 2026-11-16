function shouldBe(actual, expected) {
  if (actual !== expected)
    throw new Error(`Bad value: ${actual}!`);
}

(function() {
  function maybeReplaceEval(i) {
    if (i === (1e5 / 2)) {
      eval = otherEval;
    }
  }

  function isDirectEval() {
    return true;
  }

  var otherGlobal = createGlobalObject();
  otherGlobal.i = 0;
  otherGlobal.maybeReplaceEval = maybeReplaceEval;
  otherGlobal.isDirectEval = () => false;

  var otherEval = otherGlobal.eval;
  var eval = globalThis.eval;

  for (var i = 0; i < testLoopCount; i++) {
    eval("maybeReplaceEval(i)");
    shouldBe(eval("isDirectEval()"), i < (1e5 / 2));
  }
})();
