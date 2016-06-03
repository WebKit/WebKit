count = createBuiltin("(function () { return @argumentCount(); })");
countNoInline = createBuiltin("(function () { return @argumentCount(); })");
noInline(countNoInline);


function inlineCount() { return count(); }
noInline(inlineCount);

function inlineCount1() { return count(1); }
noInline(inlineCount1);

function inlineCount2() { return count(1,2); }
noInline(inlineCount2);

function inlineCountVarArgs(list) { return count(...list); }
noInline(inlineCountVarArgs);

function assert(condition, message) {
    if (!condition)
        throw new Error(message);
}

for (i = 0; i < 1000000; i++) {
    assert(count(1,1,2) === 3, i);
    assert(count() === 0, i);
    assert(count(1) === 1, i);
    assert(count(...[1,2,3,4,5]) === 5, i);
    assert(count(...[]) === 0, i);
    assert(inlineCount() === 0, i);
    assert(inlineCount1() === 1, i);
    assert(inlineCount2() === 2, i);
    assert(inlineCountVarArgs([1,2,3,4]) === 4, i);
    assert(inlineCountVarArgs([]) === 0, i);
    // Insert extra junk so that inlineCountVarArgs.arguments.length !== count.arguments.length
    assert(inlineCountVarArgs([1], 2, 4) === 1, i);
    assert(countNoInline(4) === 1, i)
    assert(countNoInline() === 0, i);
}
