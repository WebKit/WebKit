// Test that source positions are tracked correctly in the lazy parsing of functions.
// Lazy parsing begins in the middle of a line, at the opening parenthesis of the
// parameter list. Despite that, source positions should reflect positions in the full text.

function assert(condition, message) {
    if (!condition)
        throw new Error(message);
}

function getFirstFrameLocation(e) {
    var frame = e.stack.split('\n')[0];
    var match = frame.match(/:(\d+):(\d+)$/);
    assert(match, "Could not parse location from stack frame: " + frame);
    return { line: parseInt(match[1]), column: parseInt(match[2]) };
}

function checkLocation(e, expectedLine, expectedColumn, label) {
    var loc = getFirstFrameLocation(e);
    assert(loc.line === expectedLine && loc.column === expectedColumn,
        label + ": expected " + expectedLine + ":" + expectedColumn + ", got " + loc.line + ":" + loc.column);
}

// Case 1: throw on the same line as the function start.
var f1 = function() { throw new Error(); };
try { f1(); } catch(e) {
    checkLocation(e, 24, 38, "Case 1");
}

// Case 2: throw on its own line.
var f2 = function() {
    throw new Error();
};
try { f2(); } catch(e) {
    checkLocation(e, 31, 20, "Case 2");
}

// Case 3: nested functions, everything on the same line.
var f3 = function() { var g = function() { throw new Error(); }; return g; };
try { f3()(); } catch(e) {
    checkLocation(e, 38, 59, "Case 3");
}

// Case 4: nested functions, inner throw on a different line.
var f4 = function() {
    var g = function() { throw new Error(); };
    return g;
};
try { f4()(); } catch(e) {
    checkLocation(e, 45, 41, "Case 4");
}

// Case 5: three-level nesting, all on the same line.
var f5 = function() { var g = function() { var h = function() { throw new Error(); }; return h; }; return g; };
try { f5()()(); } catch(e) {
    checkLocation(e, 53, 80, "Case 5");
}

// Case 6: three-level nesting, each on its own line.
var f6 = function() {
    var g = function() {
        var h = function() { throw new Error(); };
        return h;
    };
    return g;
};
try { f6()()(); } catch(e) {
    checkLocation(e, 61, 45, "Case 6");
}
