description("Test WebSocket bad sub-protocol names (empty).");

// Fails if protocol is an empty string.
shouldThrow('new WebSocket("ws://127.0.0.1:8880/simple", "")');

var successfullyParsed = true;
isSuccessfullyParsed();
