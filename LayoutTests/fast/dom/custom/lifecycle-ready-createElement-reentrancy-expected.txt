This test ensures that the lifecycle callback of a parser-made element is visible in following script block.

On success, you will see a series of "PASS" messages, followed by "TEST COMPLETE".


PASS window.callbacksCalled is []
PASS window.callbacksCalled is ['X-FOO']
PASS window.callbacksCalled is ['X-FOO', 'X-BAR']
PASS window.callbacksCalled is ['X-FOO', 'X-BAR']
PASS successfullyParsed is true

TEST COMPLETE

