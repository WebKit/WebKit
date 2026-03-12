//@ skip if $memoryLimited or $addressBits <= 32
//@ runDefault()

// Testcase: regExpProtoFuncReplace accumulatedResult StringBuilder
// uses CrashOnOverflow instead of RecordOverflow.
//
// Variant of rdar://171058069 / https://bugs.webkit.org/show_bug.cgi?id=308836
//
// The accumulatedResult StringBuilder at RegExpPrototype.cpp:1049 uses
// the default CrashOnOverflow policy. The hasOverflowed() checks at
// lines 1178 and 1187 are dead code because CRASH() fires first.
//
// This testcase forces the generic slow path (regExpProtoFuncReplace)
// by overriding RegExp.prototype.exec, then uses functional replace
// to completely bypass getSubstitution() (which was already fixed).
// The accumulated result overflows from many large replacements.
//
// Expected: Catches an out-of-memory exception
// Actual (buggy): Crashes in StringBuilder::didOverflow() -> CRASH()

// Step 1: Force the generic slow path by invalidating
// regExpPrimordialPropertiesWatchpointSet.
var origExec = RegExp.prototype.exec;
RegExp.prototype.exec = function(s) { return origExec.call(this, s); };

// Step 2: Build a large replacement string (~8MB).
var bigRepl = "X";
for (var i = 0; i < 23; i++)
    bigRepl = bigRepl + bigRepl;
// bigRepl.length = 2^23 = 8388608

// Step 3: Source string with 300 matchable characters.
// 300 * 8MB = 2.4GB > String::MaxLength (2^31-1 = ~2.1GB)
var src = "A".repeat(300);

// Step 4: Match each character individually.
var re = /A/g;

var caught = false;
try {
    src.replace(re, function() { return bigRepl; });
} catch (e) {
    caught = true;
    if(e != "RangeError: Out of memory") throw new RuntimeError("unexpected exception "+e);
}
if(!caught) throw new RuntimeError("expected out of memory error");
