//@ requireOptions("--useEagerIIFEParsing=true")

// Column numbers inside eagerly parsed IIFEs must be tracked correctly. That is not a given
// because on the eager parsing path the start offset is absolute and not relative to the
// enclosing function start as it is during the normal lazy parse.

function assert(condition, message) {
    if (!condition)
        throw new Error(message);
}

function getFirstFrameColumn(e) {
    var line = e.stack.split('\n')[0];
    var match = line.match(/:(\d+)$/);
    assert(match, "Could not parse column from stack frame: " + line);
    return parseInt(match[1]);
}

// Case 1: same-line nested function in IIFE.
try { (function() { (function() { throw new Error(); })(); })(); } catch(e) { assert(getFirstFrameColumn(e) === 50, "Case 1: expected column 50, got " + getFirstFrameColumn(e)); }

// Case 2: multi-line — nested function on a different line than IIFE's '('.
try {
    (function() {
        (function() { throw new Error(); })();
    })();
} catch(e) { assert(getFirstFrameColumn(e) === 38, "Case 2: expected column 38, got " + getFirstFrameColumn(e)); }

// Case 3: deeper nesting — IIFE inside IIFE, same line.
try { (function() { (function() { (function() { throw new Error(); })(); })(); })(); } catch(e) { assert(getFirstFrameColumn(e) === 64, "Case 3: expected column 64, got " + getFirstFrameColumn(e)); }

// Case 4: multi-line, inner function offset within the line.
try {
    (function() {
        var x = 1; (function() { throw new Error(); })();
    })();
} catch(e) { assert(getFirstFrameColumn(e) === 49, "Case 4: expected column 49, got " + getFirstFrameColumn(e)); }
