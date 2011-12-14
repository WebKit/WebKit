description("Tests that the following classes are not manipulable by JavaScript (NoInterfaceObject).");

function test(name)
{
    shouldBe('typeof ' + name, '"undefined"');
    shouldThrow(name + '.prototype');
}

test('NavigatorUserMedia');
test('NavigatorUserMediaError');
test('NavigatorUserMediaSuccessCallback');
test('NavigatorUserMediaErrorCallback');

window.jsTestIsAsync = false;
window.successfullyParsed = true;
