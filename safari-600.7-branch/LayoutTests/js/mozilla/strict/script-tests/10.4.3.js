/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var obj = {}

function strict() { "use strict"; return this; }
assertEq(strict.call(""), "");
assertEq(strict.call(true), true);
assertEq(strict.call(42), 42);
assertEq(strict.call(null), null);
assertEq(strict.call(undefined), undefined);
assertEq(strict.call(obj), obj);
assertEq(new strict() instanceof Object, true);

/* 
 * The compiler internally converts x['foo'] to x.foo. Writing x[s] where
 * s='foo' is enough to throw it off the scent for now.
 */
var strictString = 'strict';

Boolean.prototype.strict = strict;
assertEq(true.strict(), true);
assertEq(true[strictString](), true);

Number.prototype.strict = strict;
assertEq((42).strict(), 42);
assertEq(42[strictString](), 42);

String.prototype.strict = strict;
assertEq("".strict(), "");
assertEq(""[strictString](), "");

function lenient() { return this; }
assertEq(lenient.call("") instanceof String, true);
assertEq(lenient.call(true) instanceof Boolean, true);
assertEq(lenient.call(42) instanceof Number, true);
assertEq(lenient.call(null), this);
assertEq(lenient.call(undefined), this);
assertEq(lenient.call(obj), obj);
assertEq(new lenient() instanceof Object, true);

var lenientString = 'lenient';

Boolean.prototype.lenient = lenient;
assertEq(true.lenient() instanceof Boolean, true);
assertEq(true[lenientString]() instanceof Boolean, true);

Number.prototype.lenient = lenient;
assertEq(42[lenientString]() instanceof Number, true);

String.prototype.lenient = lenient;
assertEq(""[lenientString]() instanceof String, true);

reportCompare(true, true);

var successfullyParsed = true;
