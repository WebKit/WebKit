description("Test WebSocket URL parsing.");

// Invalid url will fail to be parsed.
shouldThrow('new WebSocket("ws://javascript:a")');

// Can't use relative URLs - because spec says so, and because the scheme is different anyway.
shouldThrow('new WebSocket("/applet")');

// Non ws URL is not allowed.
shouldThrow('new WebSocket("javascript:a")');

// UA is allowed to block access to some ports, which we do.
shouldThrow('new WebSocket("ws://127.0.0.1:25/")');

// Resolve the url string using the resolve a Web address algorithm.
// Use 127.0.0.1:8880 and existing ws handler to make sure we don't receive unexpected response (so no console message appears)
shouldBe('(new WebSocket("ws://127.0.0.1:8880/a/../websocket/tests/simple")).URL', '"ws://127.0.0.1:8880/websocket/tests/simple"');
shouldBe('(new WebSocket("ws://127.0.0.1:8880/websocket/tests/simple?")).URL', '"ws://127.0.0.1:8880/websocket/tests/simple?"');
shouldBe('(new WebSocket("ws://127.0.0.1:8880/websocket/tests/simple?k=v")).URL', '"ws://127.0.0.1:8880/websocket/tests/simple?k=v"');

// draft-hixie-thewebsocketprotocol-60 says If /url/ has a <fragment>
// component, then fail the parsing Web Socket URLs, so throw a SYNTAX_ERR
// exception.
shouldThrow('new WebSocket("ws://127.0.0.1/path#")');
shouldThrow('new WebSocket("ws://127.0.0.1/path#fragment")');

var successfullyParsed = true;
isSuccessfullyParsed();
