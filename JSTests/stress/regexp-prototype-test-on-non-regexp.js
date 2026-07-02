//@ runDefault("--validateOptions=true", "--useConcurrentJIT=0")

// RegExpTestIntrinsic guards the receiver with Check(RegExpObjectUse), which
// fails with a BadType exit when the receiver is not a RegExpObject. The
// recompile must not re-inline the intrinsic and loop, and must keep producing
// the spec result via the runtime path.

function test(regexp, string)
{
    return regexp.test(string);
}
noInline(test);

var fake = Object.create(RegExp.prototype);
fake.exec = function() { return null; };
fake.lastIndex = 0;

for (var i = 0; i < 1e5; ++i) {
    var result = test(fake, "abc");
    if (result !== false)
        throw new Error("bad result at " + i + ": " + result);
}

if (numberOfDFGCompiles(test) > 2)
    throw new Error("too many recompiles: " + numberOfDFGCompiles(test));
