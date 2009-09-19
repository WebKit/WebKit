description("Exceptions thrown in javascript URLs should not propagate to the main script.")

var subframe = document.createElement("iframe");
document.body.appendChild(subframe);

var caughtException = false;

// Runtime exception.
try {
    subframe.src = 'javascript:throw 42';
} catch(e) {
    caughtException = true;
}
shouldBeFalse('caughtException');

// Compile-time exception.
try {
    subframe.src = 'javascript:<html></html>';
} catch(e) {
    caughtException = true;
}
shouldBeFalse('caughtException');

var successfullyParsed = true;
