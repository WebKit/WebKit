//@ runDefault("--validateOptions=true", "--useConcurrentJIT=0")

// FixupPhase guards StringReplace(RegExp) with a CheckStructure on the original
// RegExp structure. Passing a RegExp that carries an unrelated own property fails
// that check (BadCache exit). The recompile must not loop and must keep producing
// the spec result via the runtime path.

function replace(string, regexp, replacement)
{
    return string.replace(regexp, replacement);
}
noInline(replace);

var regexp = /a/;
regexp.unrelatedOwnProperty = 42;

for (var i = 0; i < 1e5; ++i) {
    var result = replace("abc", regexp, "X");
    if (result !== "Xbc")
        throw new Error("bad result at " + i + ": " + result);
}

if (numberOfDFGCompiles(replace) > 2)
    throw new Error("too many recompiles: " + numberOfDFGCompiles(replace));
