description("Test WebSocket URL parsing.");

// Can't use relative URLs - because spec says so, and because the scheme is different anyway.
shouldThrow('new WebSocket("/applet")');

// UA is allowed to block access to some ports, which we do.
shouldThrow('new WebSocket("ws://127.0.0.1:25/")');

// This is what we currently do, but not what the spec says (as of Editor's Draft 1 December 2009).
shouldBe('(new WebSocket("ws://127.0.0.1/a/../")).URL', '"ws://127.0.0.1/"');

shouldBe('(new WebSocket("ws://127.0.0.1/path?")).URL', '"ws://127.0.0.1/path?"');
shouldBe('(new WebSocket("ws://127.0.0.1/path?k=v")).URL', '"ws://127.0.0.1/path?k=v"');

// draft-hixie-thewebsocketprotocol-60 says If /url/ has a <fragment>
// component, then fail the parsing Web Socket URLs, so throw a SYNTAX_ERR
// exception.
shouldThrow('new WebSocket("ws://127.0.0.1/path#")');
shouldThrow('new WebSocket("ws://127.0.0.1/path#fragment")');

var successfullyParsed = true;
isSuccessfullyParsed();
