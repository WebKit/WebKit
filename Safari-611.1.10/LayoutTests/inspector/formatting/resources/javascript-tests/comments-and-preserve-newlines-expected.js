// TOP

// This is a line comment.
1;

/* This is a block comment */
2;

// Line - before
/* Block */
// Line - after
3;

/* Block - before */
// Line
/* Block - after */
4;

/*
 * Multiline Block
 */
5;

// ---------------
//   Line Header
// ---------------
6;

7; // Comment on line with code.

// Leading comment for x.
var x = 1; // Trailing comment for x.
// Leading comment for y.
var y = 2; // Trailing comment for y.

var x = 1; // Trailing comment for x.
var y = 2; // Trailing comment for y.

// Leading comment for x.
var x = 1;
// Leading comment for y.
var y = 2;

/* Alpha */
var x = 1; /* Alpha */ /* Beta */
/* Gamma */

(function() {
    // Increment.
    x++;
    y++;

    // Sum the list.
    let sum = 0;
    for (let x of list)
        sum += x;

    // Return result.
    return sum;
})();

// BOTTOM
