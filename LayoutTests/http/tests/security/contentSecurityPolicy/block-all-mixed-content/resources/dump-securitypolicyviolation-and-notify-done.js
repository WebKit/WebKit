var consoleElement; // Only used if !window.opener.
var recordedSecurityPolicyViolation;
var didFireLoad = false;

function logMessage(message)
{
    console.assert(consoleElement);
    consoleElement.appendChild(document.createTextNode(message + "\n"));
}

function securityPolicyViolationToString()
{
    let lines = [];
    for (let key in recordedSecurityPolicyViolation)
        lines.push(key + ": " + recordedSecurityPolicyViolation[key]);
    lines.push("");
    return lines.join("\n");
}

function checkNotify()
{
    if (!didFireLoad || !recordedSecurityPolicyViolation)
        return;
    if (window.opener) {
        // window.opener is responsible for calling testRunner.notifyDone().
        opener.postMessage(securityPolicyViolationToString(), "*");
    } else {
        logMessage(securityPolicyViolationToString());
        if (window.testRunner)
            testRunner.notifyDone();
    }
}

function recordSecurityPolicyViolation(e)
{
    document.removeEventListener("securitypolicyviolation", recordSecurityPolicyViolation, false);

    let keysToDump = [
        "documentURI",
        "referrer",
        "blockedURI",
        "violatedDirective",
        "effectiveDirective",
        "originalPolicy",
        "sourceFile",
        "lineNumber",
        "columnNumber",
        "statusCode",
    ];
    let result = { };
    for (let key of keysToDump)
        result[key] = e[key];
    recordedSecurityPolicyViolation = result;
    checkNotify();
}

document.addEventListener("securitypolicyviolation", recordSecurityPolicyViolation, false);

window.onload = function ()
{
    if (!window.opener) {
        consoleElement = document.createElement("pre");
        document.body.appendChild(consoleElement);
    }
    didFireLoad = true;
    checkNotify();
}
