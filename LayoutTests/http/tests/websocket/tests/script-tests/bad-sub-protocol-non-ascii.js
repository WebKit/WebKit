description("Test WebSocket bad sub-protocol names by non-ASCII chars.");

// Fails if protocol contains an character greater than U+007E.
shouldThrow('new WebSocket("ws://127.0.0.1:8880/simple", "\u007F")');
shouldThrow('new WebSocket("ws://127.0.0.1:8880/simple", "\u0080")');
shouldThrow('new WebSocket("ws://127.0.0.1:8880/simple", "\u3042")');
shouldThrow('new WebSocket("ws://127.0.0.1:8880/simple", "\uFFFF")');
shouldThrow('new WebSocket("ws://127.0.0.1:8880/simple", "\uFEFF")');
shouldThrow('new WebSocket("ws://127.0.0.1:8880/simple", "\uFFFE")');
// Surrogate pairs
shouldThrow('new WebSocket("ws://127.0.0.1:8880/simple", "\uD840\uDC0B")');

var successfullyParsed = true;
isSuccessfullyParsed();
