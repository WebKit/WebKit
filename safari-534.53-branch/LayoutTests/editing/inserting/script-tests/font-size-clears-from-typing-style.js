description("Tests that font-size in typingStyle is correctly cleared. See https://bugs.webkit.org/show_bug.cgi?id=26279.");

var editable = document.createElement('div');
editable.contentEditable = true;
editable.innerHTML = '<br><div id="wrapper">A</div>';

document.body.appendChild(editable);
editable.focus();

window.getSelection().setBaseAndExtent(editable, 0, editable, 0);
document.execCommand('ForwardDelete', false, false);
document.execCommand('InsertText', false, 'B');

// The forward delete leaves the cursor in the "wrapper" div.
// After typing, there should be exactly one textnode in the wrapper div.
shouldBe(String(wrapper.childNodes.length), '1');
shouldBe(String(wrapper.firstChild.nodeType), '3');

var successfullyParsed = true;
