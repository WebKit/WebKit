function getValidationBubbleContents()
{
    return `
    (function() {
        return JSON.stringify(uiController.contentsOfUserInterfaceItem('validationBubble'));
    })();`
}

function getValidationBubble()
{
    return new Promise((resolve) => {
        setTimeout(() => {
            setTimeout(() => {
                testRunner.runUIScript(getValidationBubbleContents(), function(result) {
                    resolve(JSON.parse(result).validationBubble);
                });
            }, 0);
        }, 0);
    });
}

function getValidationMessage()
{
    return getValidationBubble().then((validationBubble) => {
        return validationBubble.message;
    });
}

