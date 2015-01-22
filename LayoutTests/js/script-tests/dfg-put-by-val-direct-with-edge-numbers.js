description(
"Test that a object accepts DFG PutByValueDirect operation with edge numbers."
);

function lookupWithKey(key) {
    var object = {
        [key]: 42
    };
    return object[key];
}

[
    // integers
    '-0x80000001',  // out of int32_t
    '-0x80000000',  // int32_t min
    '-1',           // negative
    '0',            // zero
    '1',            // positive
    '0x7fffffff',   // int32_t max
    '0x80000000',   // out of int32_t
    '0xfffffffd',   // less than array max in JSObject
    '0xfffffffe',   // array max in JSObject
    '0xffffffff',   // uint32_t max, PropertyName::NotAnIndex
    '0x100000000',  // out of uint32_t

    // stringified integers
    '"' + (-0x80000001).toString() + '"',  // out of int32_t
    '"' + (-0x80000000).toString() + '"',  // int32_t min
    '"' + (-1).toString() + '"',           // negative
    '"' + (0).toString() + '"',            // zero
    '"' + (1).toString() + '"',            // positive
    '"' + (0x7fffffff).toString() + '"',   // int32_t max
    '"' + (0x80000000).toString() + '"',   // out of int32_t
    '"' + (0xfffffffd).toString() + '"',   // less than array max in JSObject
    '"' + (0xfffffffe).toString() + '"',   // array max in JSObject
    '"' + (0xffffffff).toString() + '"',   // uint32_t max).toString() PropertyName::NotAnIndex
    '"' + (0x100000000).toString() + '"',  // out of uint32_t

    // doubles
    'Number.MIN_VALUE',
    'Number.MAX_VALUE',
    'Number.MIN_SAFE_INTEGER',
    'Number.MAX_SAFE_INTEGER',
    'Number.POSITIVE_INFINITY',
    'Number.NEGATIVE_INFINITY',
    'Number.NaN',
    'Number.EPSILON',
    '+0.0',
    '-0.0',
    '0.1',
    '-0.1',
    '4.2',
    '-4.2',

    // stringified doules
    '"' + (Number.MIN_VALUE).toString() + '"',
    '"' + (Number.MAX_VALUE).toString() + '"',
    '"' + (Number.MIN_SAFE_INTEGER).toString() + '"',
    '"' + (Number.MAX_SAFE_INTEGER).toString() + '"',
    '"' + (Number.POSITIVE_INFINITY).toString() + '"',
    '"' + (Number.NEGATIVE_INFINITY).toString() + '"',
    '"NaN"',
    '"' + (Number.EPSILON).toString() + '"',
    '"+0.0"',
    '"-0.0"',
    '"0.1"',
    '"-0.1"',
    '"4.2"',
    '"-4.2"',
].forEach(function (key) {
    dfgShouldBe(lookupWithKey, "lookupWithKey("+ key + ")", "42");
});

function dfgShouldThrow(theFunction, _a, _e)
{
  if (typeof theFunction != "function" || typeof _a != "string" || typeof _e != "string")
    debug("WARN: dfgShouldThrow() expects a function and two strings");
  noInline(theFunction);
  var values = [], _av = undefined, notThrow = false;

  // Defend against tests that muck with numeric properties on array.prototype.
  values.__proto__ = null;
  values.push = Array.prototype.push;

  while (!dfgCompiled({f:theFunction})) {
    try {
        _av = eval(_a);
        notThrow = true;
    } catch (exception) {
        values.push(exception);
    }
  }
  try {
    _av = eval(_a);
    notThrow = true;
  } catch (exception) {
    values.push(exception);
  }

  var _ev = eval(_e);
  if (notThrow) {
    if (typeof _av == "undefined")
      testFailed(_a + " should throw " + (typeof _e == "undefined" ? "an exception" : _ev) + ". Was undefined.");
    else
      testFailed(_a + " should throw " + (typeof _e == "undefined" ? "an exception" : _ev) + ". Was " + _av + ".");
  } else {
    var allPassed = true;
    for (var i = 0; i < values.length; ++i) {
      var _av = values[i];
      if (typeof _e == "undefined" || _av == _ev)
        continue;
      testFailed(_a + " should throw " + (typeof _e == "undefined" ? "an exception" : _ev) + ". Threw exception " + _av + ".");
      allPassed = false;
    }
    if (allPassed)
      testPassed(_a + " threw exception " + _e + " on all iterations including after DFG tier-up.");
  }

  return values.length;
}

function lookupWithKey2(key) {
    var object = {
        [key]: 42
    };
    return object[key];
}

var toStringThrowsError = {
    toString: function () {
        throw new Error('ng');
    }
};
dfgShouldThrow(lookupWithKey2, "lookupWithKey2(toStringThrowsError)", "'Error: ng'");
