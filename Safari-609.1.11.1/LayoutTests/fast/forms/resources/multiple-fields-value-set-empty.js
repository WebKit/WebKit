var input;
var emptyText;

function testSettingEmptyStringClearsSubFields(type) {
    description('Check if input.value="" clears an input with partially-specified value.');

    input = document.createElement('input');
    input.type = type;
    document.body.appendChild(input);
    input.focus();
    emptyText = getUserAgentShadowTextContent(input);
    if (!window.eventSender)
        debug('This test needs to be run on DRT/WTR.');
    else {
        debug('Empty text: ' + emptyText);
        shouldNotBe('eventSender.keyDown("upArrow"); getUserAgentShadowTextContent(input)', 'emptyText');
        shouldBe('input.value = ""; getUserAgentShadowTextContent(input)', 'emptyText');
        input.remove();
    }
}
