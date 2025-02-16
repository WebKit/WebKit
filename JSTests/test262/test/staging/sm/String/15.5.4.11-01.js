/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-String-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
var BUGNUMBER = 587366;
var summary = "String.prototype.replace with non-regexp searchValue";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

/* 
 * Check that regexp statics are preserved across the whole test.
 * If the engine is trying to cheat by turning stuff into regexps,
 * we should catch it!
 */
/(a|(b)|c)+/.exec('abcabc');
var before = {
    "source" : RegExp.source,
    "$`": RegExp.leftContext,
    "$'": RegExp.rightContext,
    "$&": RegExp.lastMatch,
    "$1": RegExp.$1,
    "$2": RegExp.$2
};

var text = 'I once was lost but now am found.';
var searchValue = 'found';
var replaceValue;

/* Lambda substitution. */
replaceValue = function(matchStr, matchStart, textStr) {
    assert.sameValue(matchStr, searchValue);
    assert.sameValue(matchStart, 27);
    assert.sameValue(textStr, text);
    return 'not watching that show anymore';
}
var result = text.replace(searchValue, replaceValue);
assert.sameValue(result, 'I once was lost but now am not watching that show anymore.');

/* Dollar substitution. */
replaceValue = "...wait, where was I again? And where is all my $$$$$$? Oh right, $`$&$'" +
               " But with no $$$$$$"; /* Note the dot is not replaced and trails the end. */
result = text.replace(searchValue, replaceValue);
assert.sameValue(result, 'I once was lost but now am ...wait, where was I again?' +
                 ' And where is all my $$$? Oh right, I once was lost but now am found.' +
                 ' But with no $$$.');

/* Missing capture group dollar substitution. */
replaceValue = "$1$&$2$'$3";
result = text.replace(searchValue, replaceValue);
assert.sameValue(result, 'I once was lost but now am $1found$2.$3.');

/* Check RegExp statics haven't been mutated. */
for (var ident in before)
    assert.sameValue(RegExp[ident], before[ident]);

/******************************************************************************/

print("All tests passed!");
