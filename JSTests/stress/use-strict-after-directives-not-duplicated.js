//@ requireOptions("--useDollarVM=1", "--forceDebuggerBytecodeGeneration=1")

// When "use strict" follows other directives, the parser restarts the directive prologue in strict
// mode. The directives before "use strict" must appear only once in the resulting tree. Each
// statement gets a debugger hook, so duplicated directives make the bytecode larger. A hook at the
// same position as the previous one is not emitted, so each case needs two directives before
// "use strict". Each case is compared with the same statements with "use strict" first, which
// does not restart the prologue.

var failures = [];

function bytecodeSize(codeBlockDescription) {
    var match = codeBlockDescription.match(/, (\d+)(?: \([^)]*\))?\]$/);
    if (!match)
        throw new Error(`cannot parse code block description ${codeBlockDescription}`);
    return parseInt(match[1]);
}

function checkSameSize(description, actual, expected) {
    var actualSize = bytecodeSize(actual);
    var expectedSize = bytecodeSize(expected);
    if (actualSize !== expectedSize)
        failures.push(`${description}: expected bytecode size ${expectedSize}, got ${actualSize}`);
}

var withRestart = '"a"; "b"; "use strict"; var y = 2; $vm.codeBlockFor($vm.codeBlockForFrame(0))';
var withoutRestart = '"use strict"; "a"; "b"; var y = 2; $vm.codeBlockFor($vm.codeBlockForFrame(0))';

checkSameSize("program code", $.evalScript(withRestart), $.evalScript(withoutRestart));
checkSameSize("direct eval code", eval(withRestart), eval(withoutRestart));
checkSameSize("indirect eval code", (0, eval)(withRestart), (0, eval)(withoutRestart));

if (failures.length)
    throw new Error(failures.join("\n"));
