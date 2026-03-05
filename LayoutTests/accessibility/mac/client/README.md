Client-side accessibility layout tests

These tests use the client-side accessibility API - the same one used by VoiceOver, etc.
as opposed to the normal accessibility layout tests that call into WebCore for the
current web content process directly.

This layer was added so that we have a way to test site isolation - it enables us to
walk the entire cross-process accessibility tree from one test.
