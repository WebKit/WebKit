/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */


/*
 * JSFunction::findDuplicateFormal (nee js_FindDuplicateFormal), used
 * by strict checks, sometimes failed to choose the correct branch of
 * the fun->u.i.names union: it used the argument count, not the
 * overall name count.
 */
function f(a1,a2,a3,a4,a5) { "use strict"; var v1, v2, v3, v4, v5, v6, v7; }

reportCompare(true, true);

var successfullyParsed = true;
