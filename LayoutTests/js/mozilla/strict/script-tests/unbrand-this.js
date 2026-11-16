/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/* Test JSOP_UNBRANDTHIS's behavior on object and non-object |this| values. */

function strict() {
  "use strict";
  this.insert = function(){ bar(); };
  function bar() {}
}

var exception;

// Try 'undefined' as a |this| value.
exception = null;
try {
  strict.call(undefined);
} catch (x) {
  exception = x;
}
assertEq(exception instanceof TypeError, true);

// Try 'null' as a |this| value.
exception = null;
try {
  strict.call(null);
} catch (x) {
  exception = x;
}
assertEq(exception instanceof TypeError, true);

// An object as a |this| value should be fine.
exception = null;
try {
  strict.call({});
} catch (x) {
  exception = x;
}
assertEq(exception, null);

reportCompare(true, true);

var successfullyParsed = true;
