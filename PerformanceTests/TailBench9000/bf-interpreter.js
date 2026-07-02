"use strict";

function lookForMatchingBracket(program, pc, level) {
    if (pc >= program.length)
        throw "Error: Unbalanced brackets in the BF program, too many opening brackets";
    
    switch(program[pc]) {
        case '[':
            return lookForMatchingBracket(program, pc+1, level+1);
        case ']':
            if (level == 1)
                return pc;
            return lookForMatchingBracket(program, pc+1, level-1);
        default:
            return lookForMatchingBracket(program, pc+1, level);
    }
}

// (leftTape, tapeCursor, rightTape) form a zipper:
// leftTape is the (infinite) list of all values to the left of the cursor (from right to left),
// while rightTape is the (infinite) list of all values to the right of the cursor (from left to right).
// These lists are represented as functions that return an object with the first value, and a function for the rest of the list.
function evalRec(program, pc, input, output, leftTape, tapeCursor, rightTape, loopContinuation)
{
    if (pc >= program.length)
        return output;

    switch(program[pc]) {
    case '.':
        const newOutput = output.concat(String.fromCharCode(tapeCursor));
        return evalRec(program, pc+1, input, newOutput, leftTape, tapeCursor, rightTape, loopContinuation);
    case ',':
        return evalRec(program, pc+1, input.slice(1), output, leftTape, input.charCodeAt(0), rightTape, loopContinuation);
    case '+':
        return evalRec(program, pc+1, input, output, leftTape, tapeCursor+1, rightTape, loopContinuation);
    case '-':
        return evalRec(program, pc+1, input, output, leftTape, tapeCursor-1, rightTape, loopContinuation);
    case '>':
        const evaluatedRightTape = rightTape();
        return evalRec(program, pc+1, input, output, ()=>({cursor: tapeCursor, rest: leftTape}), evaluatedRightTape.cursor, evaluatedRightTape.rest, loopContinuation);
    case '<':
        const evaluatedLeftTape = leftTape();
        return evalRec(program, pc+1, input, output, evaluatedLeftTape.rest, evaluatedLeftTape.cursor, ()=>({cursor: tapeCursor, rest: rightTape}), loopContinuation);
    case '[':
        const matchingPC = lookForMatchingBracket(program, pc, 0);
        if (tapeCursor == 0)
            return evalRec(program, matchingPC+1, input, output, leftTape, tapeCursor, rightTape, loopContinuation);
        return evalRec(program, pc+1, input, output, leftTape, tapeCursor, rightTape, (...varargs) => evalRec(program, pc, ...varargs, loopContinuation));
    case ']':
        return loopContinuation(input, output, leftTape, tapeCursor, rightTape);
    default:
        throw "Unsupported character: " + program[pc] + " at pc " + pc;
    }
}

function infiniteTape()
{
    return {cursor: 0, rest: infiniteTape};
}

function evalShort(program, input)
{
    return evalRec(program, 0, input, "", infiniteTape, 0, infiniteTape, ()=>{throw "Error: Unbalanced brackets in the BF program, too many closing brackets";});
}

function test(program, input, expectedOutput)
{
    const result = evalShort(program, input);
    if (result != expectedOutput)
        throw ("Wrong result, program: " + program + " on input " + input + " had output " + result + " instead of " + expectedOutput);
}

for (var i = 0; i < 50000; ++i) {
    test(",...", "A", "AAA");
    test(",..+..--.", "C", "CCDDB");
    test(",<,>..<..", "EF", "EEFF");
    // The following program is taken from the Wikipedia brainfuck page:
    test("++++++++++[>+++++++>++++++++++>+++>+<<<<-]>++.>+.+++++++..+++.>++.<<+++++++++++++++.>.+++.------.--------.>+.>.", "", "Hello World!\n")
}
