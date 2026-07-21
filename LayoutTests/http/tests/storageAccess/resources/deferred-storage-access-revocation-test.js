// Shared driver for the "deferred storage-access revocation" tests. Each test grants a
// cross-site iframe storage access, then has the cross-site top frame initiate a
// navigation of that iframe and checks that storage access is retained.
//
// This testing code is very similar to the code from request-and-grant-access-cross-origin-sandboxed-iframe-*
//
// Because revocation should happen at commit time, an iframe that is has a navigation that is 
// in-flight or never commits should not have its storage access removed.
//
// Before including this script, each test need to define:
//   navigationTarget      - Path (everything after the origin) of where the top frame navigates the iframe to in step 4
//   navigationDescription - debug() line logged when that navigation is initiated.

jsTestIsAsync = true;

// The cross-site iframe that is granted, then expected to retain, storage access. Everything
// below is derived from this one origin; the top frame is this test page's own origin.
const thirdPartyOrigin = "https://localhost:8443";
const thirdPartyResourcesUrl = thirdPartyOrigin + "/storageAccess/resources";

// Identifies the third-party host to the tracking-prevention (ITP) APIs. These key on the
// registrable domain, so the "/temp" path is an unused placeholder.
const statisticsUrl = thirdPartyOrigin + "/temp";
const firstPartyCookieName = "firstPartyCookie";
const subPathToSetFirstPartyCookie = "/set-cookie.py?name=" + firstPartyCookieName + "&value=value";
const returnUrl = document.location.href.split("#")[0];

function activateElement(elementId) {
    let element = document.getElementById(elementId);
    let centerX = element.offsetLeft + element.offsetWidth / 2;
    let centerY = element.offsetTop + element.offsetHeight / 2;
    UIHelper.activateAt(centerX, centerY).then(
        function () {
            if (window.eventSender)
                eventSender.keyDown("escape");
            else {
                testFailed("No eventSender.");
                setEnableFeature(false, finishJSTest);
            }
        },
        function () {
            testFailed("Promise rejected.");
            setEnableFeature(false, finishJSTest);
        }
    );
}

function relayMessage(data) {
    if (data.indexOf("PASS") === 0)
        testPassed(data.replace(/^PASS[.]? ?/, ""));
    else
        testFailed(data);
}

function receiveMessage(event) {
    if (event.origin === thirdPartyOrigin)
        relayMessage(event.data);
    else
        testFailed("Received a message from an unexpected origin: " + event.origin);
    runTest();
}

async function runTest() {
    switch (document.location.hash) {
        case "#step1":
            if (testRunner.isStatisticsPrevalentResource(statisticsUrl))
                testFailed("Host prematurely set as prevalent resource.");
            // Set first-party cookie for localhost.
            document.location.href = thirdPartyResourcesUrl + subPathToSetFirstPartyCookie + "#" + returnUrl + "#step2";
            break;
        case "#step2":
            document.location.hash = "step3";
            // Set localhost as prevalent with user interaction so its cookies are blocked in a third-party context.
            testRunner.setStatisticsHasHadUserInteraction(statisticsUrl, true, function() {
                testRunner.setStatisticsPrevalentResource(statisticsUrl, true, function() {
                    testRunner.statisticsUpdateCookieBlocking(runTest);
                });
            });
            break;
        case "#step3":
            document.location.hash = "step4";
            // Per-frame access is required for clearing access on navigation of the iframe.
            await testRunner.setStorageAccessAPIPerPageScopeEnabled(false);
            // Create a sandboxed iframe that will request and be granted storage access.
            let iframeElement = document.createElement("iframe");
            iframeElement.setAttribute("sandbox", "allow-storage-access-by-user-activation allow-scripts allow-same-origin allow-modals");
            iframeElement.onload = function() {
                testRunner.statisticsUpdateCookieBlocking(function() {
                    activateElement("TheIframeThatRequestsStorageAccess");
                });
            };
            iframeElement.id = "TheIframeThatRequestsStorageAccess";
            iframeElement.src = thirdPartyResourcesUrl + "/request-storage-access-and-report-client-side-cookies-iframe.html";
            document.body.appendChild(iframeElement);
            break;
        case "#step4":
            document.location.hash = "step5";
            // The cross-site top frame initiates a navigation of the iframe. 
            // The iframe's JavaScript will check if it retain storage 
            // access after a failed load commit.
            let iframe = document.getElementById("TheIframeThatRequestsStorageAccess");
            iframe.src = thirdPartyOrigin + navigationTarget;
            debug(navigationDescription);
            setTimeout(function() {
                iframe.contentWindow.postMessage("reportBackCookies", thirdPartyOrigin);
            }, 0);
            break;
        case "#step5":
            // Remove the iframe to cancel the pending navigation so the page finishes loading
            // before notifyDone.
            document.getElementById("TheIframeThatRequestsStorageAccess").remove();
            // Reset access scope.
            await testRunner.setStorageAccessAPIPerPageScopeEnabled(true);
            setEnableFeature(false, finishJSTest);
            break;
    }
}

if (document.location.hash === "") {
    setEnableFeature(true, function() {
        document.location.hash = "step1";
    });
}

window.addEventListener("message", receiveMessage, false);

runTest();
