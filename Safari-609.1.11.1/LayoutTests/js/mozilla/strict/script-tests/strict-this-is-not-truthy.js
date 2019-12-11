// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

// See bug 630543.

function f() {
    "use strict";
    return !this;
}
assertEq(f.call(null), true);

reportCompare(0, 0, 'ok');

var successfullyParsed = true;
