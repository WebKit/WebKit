//@ skip if ["arm", "mips"].include?($architecture)
// This test checks for proper incrementing around / over individual surrogates and surrogate pairs.
// This test should run without crashing.

function testRegExpMatch(re, str)
{
    for (let i = 0; i < 100; ++i) {
        let match = re.exec(str);
        if (!match || match[0] != str) {
            print(match);
            throw "Expected " + re + " to match \"" + str + "\" but it didn't";
        }
    }
}

let testString = "\ud800\ud800\udc00";
let greedyRegExp = /([^x]+)[^]*\1([^])/u;

testRegExpMatch(greedyRegExp, testString);

let nonGreedyRegExp = /(.*[^x]+?)[^]*([^])/u;

testRegExpMatch(nonGreedyRegExp, testString);

let testString2 = "\ud800\ud800\udc00Test\udc00123";
let backtrackGreedyRegExp = /.*[\x20-\udffff].\w*.\d{3}/u;

testRegExpMatch(backtrackGreedyRegExp, testString2);
