description("Test WebSocket bad sub-protocol names by control characters.");

// Fails if protocol contains an character less than U+0020.
shouldThrow('new WebSocket("ws://127.0.0.1:8880/simple", "\u0000")');
shouldThrow('new WebSocket("ws://127.0.0.1:8880/simple", "\u0009")');
shouldThrow('new WebSocket("ws://127.0.0.1:8880/simple", "\u000A")');
shouldThrow('new WebSocket("ws://127.0.0.1:8880/simple", "\u000D")');
shouldThrow('new WebSocket("ws://127.0.0.1:8880/simple", "\u001B")');

var successfullyParsed = true;
isSuccessfullyParsed();
