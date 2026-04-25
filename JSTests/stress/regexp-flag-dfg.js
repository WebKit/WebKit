// Test that RegExp flag getters produce correct results when compiled by DFG/FTL.

function testFlag(re, flagName, expected) {
    var result = re[flagName];
    if (result !== expected)
        throw new Error("Expected " + flagName + " to be " + expected + " but got " + result + " for " + re);
}

function testGlobal(re, expected) { return re.global; }
function testIgnoreCase(re, expected) { return re.ignoreCase; }
function testMultiline(re, expected) { return re.multiline; }
function testDotAll(re, expected) { return re.dotAll; }
function testSticky(re, expected) { return re.sticky; }
function testUnicode(re, expected) { return re.unicode; }
function testUnicodeSets(re, expected) { return re.unicodeSets; }
function testHasIndices(re, expected) { return re.hasIndices; }

noInline(testGlobal);
noInline(testIgnoreCase);
noInline(testMultiline);
noInline(testDotAll);
noInline(testSticky);
noInline(testUnicode);
noInline(testUnicodeSets);
noInline(testHasIndices);

var reAll = /foo/dgimsuy;
var reNone = /foo/;

for (var i = 0; i < 1e5; ++i) {
    // Test with all flags set
    if (testGlobal(reAll) !== true) throw new Error("global should be true");
    if (testIgnoreCase(reAll) !== true) throw new Error("ignoreCase should be true");
    if (testMultiline(reAll) !== true) throw new Error("multiline should be true");
    if (testDotAll(reAll) !== true) throw new Error("dotAll should be true");
    if (testSticky(reAll) !== true) throw new Error("sticky should be true");
    if (testUnicode(reAll) !== true) throw new Error("unicode should be true");
    if (testHasIndices(reAll) !== true) throw new Error("hasIndices should be true");

    // Test with no flags set
    if (testGlobal(reNone) !== false) throw new Error("global should be false");
    if (testIgnoreCase(reNone) !== false) throw new Error("ignoreCase should be false");
    if (testMultiline(reNone) !== false) throw new Error("multiline should be false");
    if (testDotAll(reNone) !== false) throw new Error("dotAll should be false");
    if (testSticky(reNone) !== false) throw new Error("sticky should be false");
    if (testUnicode(reNone) !== false) throw new Error("unicode should be false");
    if (testUnicodeSets(reNone) !== false) throw new Error("unicodeSets should be false");
    if (testHasIndices(reNone) !== false) throw new Error("hasIndices should be false");
}

// Test unicodeSets separately (can't combine with unicode)
var reV = /foo/v;
for (var i = 0; i < 1e5; ++i) {
    if (testUnicodeSets(reV) !== true) throw new Error("unicodeSets should be true");
    if (testUnicode(reV) !== false) throw new Error("unicode should be false for v flag");
}
