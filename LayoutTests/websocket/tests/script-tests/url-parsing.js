description("Test WebSocket URL parsing.");

// Can't use relative URLs - because spec says so, and because the scheme is different anyway.
shouldThrow('new WebSocket("/applet")');

// UA is allowed to block access to some ports, which we do.
shouldThrow('new WebSocket("ws://127.0.0.1:25/")');

// This is what we currently do, but not what the spec says (as of Editor's Draft 1 December 2009).
// The spec says that the string passed to WebScoket constructor should be returned unchanged.
shouldBe('(new WebSocket("ws://127.0.0.1/a/../")).URL', '"ws://127.0.0.1/"');

var successfullyParsed = true;
isSuccessfullyParsed();
