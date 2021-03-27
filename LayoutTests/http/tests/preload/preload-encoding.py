#!/usr/bin/env python3

import sys

sys.stdout.write(
    'Link: <resources/success.js>; rel=preload; as=script\r\n'
    'Content-Type: text/html\r\n\r\n'
)

print('''<script src="../../resources/js-test-pre.js"></script>
<script>
jsTestIsAsync = true;

description("First load a script with a wrong charset then again with the right one. Second attempt should work and 'scriptSuccess' should be set to true. 'successfullyParsed' will be undefined.");

function appendScriptWithCharset(charset, onload)
{
    var script = document.createElement("script");
    script.src = "resources/success.js";
    script.setAttribute("charset", charset);
    script.onload = onload;
    script.onerror = onload;
    document.body.appendChild(script);
}

function test()
{
    appendScriptWithCharset("utf-16", function () {
        appendScriptWithCharset("utf-8", function () {
            shouldBeTrue("scriptSuccess");
            finishJSTest();
        });
    });
}
</script>
<body onload="test()">
<script src="../../resources/js-test-post.js"></script>''')