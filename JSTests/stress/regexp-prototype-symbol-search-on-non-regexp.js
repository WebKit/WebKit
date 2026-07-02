//@ runDefault("--validateOptions=true", "--useConcurrentJIT=0")

// RegExpSearchIntrinsic guards the receiver with Check(RegExpObjectUse), which
// fails with a BadType exit when the receiver is not a RegExpObject. The
// recompile must not re-inline the intrinsic and loop, and must keep producing
// the spec result via the runtime path.

function search(regexp, string)
{
    return regexp[Symbol.search](string);
}
noInline(search);

var fake = Object.create(RegExp.prototype);
fake.exec = function() { return null; };
fake.lastIndex = 0;

for (var i = 0; i < 1e5; ++i) {
    var result = search(fake, "abc");
    if (result !== -1)
        throw new Error("bad result at " + i + ": " + result);
}

if (numberOfDFGCompiles(search) > 2)
    throw new Error("too many recompiles: " + numberOfDFGCompiles(search));
