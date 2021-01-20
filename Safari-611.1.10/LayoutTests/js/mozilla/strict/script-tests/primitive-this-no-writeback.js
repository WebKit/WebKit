/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/* Verify that GETTHISPROP does not update the frame's |this| slot. */

var f = String.prototype.m = function () {
    "use strict";
    assertEq(this, "s");
    // The GETTHISPROP should not cause |this| to become wrapped.
    return [this.m, this];
};
var a = "s".m();
assertEq(a[0], f);
assertEq(a[1], "s");

reportCompare(true, true);

var successfullyParsed = true;
