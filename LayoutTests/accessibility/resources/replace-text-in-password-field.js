// Shared by replace-text-in-password-field.html and isolated-tree/replace-text-in-password-field.html.
// The page needs an input of type password with id "password", and sets waitsForIsolatedTree in isolated tree mode.

var output = "Tests replacing text in a password field the way VoiceOver does when typing with braille screen input: replacing the last N characters of the value.\n\n";

function axValueLength() {
    return axPassword.stringValue.replace(/^AXValue: /, "").length;
}

// The iOS accessibility bundle reads the field's accessibility value right after replacing text, so without the isolated
// tree it must already be up to date. The isolated tree can be updated later: a password field's value change can reach it
// only with the field's value-changed notification, which is posted on an interval. So in that mode, wait for it.
async function waitForAXValueUpdate() {
    if (window.waitsForIsolatedTree)
        await waitFor(() => axValueLength() === password.value.length);
}

// Like the iOS accessibility bundle, compute the range from the length of the field's accessibility value, which is masked.
function replaceAtEnd(charactersToRemove, replacement) {
    const length = axValueLength();
    return axPassword.replaceTextInRange(replacement, length - charactersToRemove, charactersToRemove);
}

function setValue(value) {
    password.value = value;
}

var axPassword;
var password;
if (window.accessibilityController) {
    window.jsTestIsAsync = true;

    axPassword = accessibilityController.accessibleElementById("password");
    password = document.getElementById("password");
    password.focus();

    setTimeout(async function() {
        // Typing "foo bar". Each new cell replaces the characters braille screen input already put in the field for the current word.
        for (const [charactersToRemove, replacement] of [[0, "f"], [1, "fo"], [2, "foo"], [0, " "], [0, "b"], [1, "ba"], [2, "bar"]]) {
            await waitForAXValueUpdate();
            output += expect(`replaceAtEnd(${charactersToRemove}, '${replacement}')`, "true");
        }
        output += expect("password.value", "'foo bar'");
        // The client sets the caret from the accessibility value's length right after each replacement, so it must be up to date.
        await waitForAXValueUpdate();
        output += expect("axValueLength()", "password.value.length");

        // A contraction at a word break replaces the cell with the whole word and a space.
        setValue("foo b");
        await waitForAXValueUpdate();
        output += expect("replaceAtEnd(1, 'but ')", "true");
        output += expect("password.value", "'foo but '");
        await waitForAXValueUpdate();
        output += expect("axValueLength()", "password.value.length");

        // Deleting one character.
        setValue("foo");
        await waitForAXValueUpdate();
        output += expect("replaceAtEnd(1, '')", "true");
        output += expect("password.value", "'fo'");
        await waitForAXValueUpdate();
        output += expect("axValueLength()", "password.value.length");

        // Deleting a word. The value is masked, so the client can't find word boundaries and removes everything.
        setValue("foo bar");
        await waitForAXValueUpdate();
        output += expect("replaceAtEnd(axValueLength(), '')", "true");
        output += expect("password.value", "''");
        await waitForAXValueUpdate();
        output += expect("axValueLength()", "0");

        // Replacing in the middle of the value.
        setValue("abcdef");
        output += expect("axPassword.replaceTextInRange('XY', 2, 2)", "true");
        output += expect("password.value", "'abXYef'");

        debug(output);
        finishJSTest();
    }, 0);
}
