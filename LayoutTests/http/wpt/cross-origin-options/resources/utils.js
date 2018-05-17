const RESOURCES_DIR = "/WebKit/cross-origin-options/resources/";

function isCrossOriginWindow(w)
{
    try {
        w.name;
    } catch (e) {
        return true;
    }
    return false;
}

async function waitForCrossOriginLoad(w)
{
    return new Promise((resolve) => {
        window.addEventListener('message', (msg) => {
            if (msg.source != w || msg.data != "READY")
                return;
            resolve();
        });

        let handle = setInterval(() => {
            if (isCrossOriginWindow(w)) {
                clearInterval(handle);
                try {
                    w.postMessage;
                } catch (e) {
                    // No point in waiting for "READY" message from window since postMessage is
                    // not available.
                    resolve();
                }
            }
        }, 5);
    });
}

async function withIframe(resourceFile, crossOrigin)
{
    return new Promise((resolve) => {
        let resourceURL = crossOrigin ? get_host_info().HTTP_REMOTE_ORIGIN : get_host_info().HTTP_ORIGIN;
        resourceURL += RESOURCES_DIR;
        resourceURL += resourceFile;
        let frame = document.createElement("iframe");
        frame.src = resourceURL;
        if (crossOrigin) {
            document.body.appendChild(frame);
            waitForCrossOriginLoad(frame.contentWindow).then(() => {
                resolve(frame);
            });
        } else {
            frame.onload = function() { resolve(frame); };
            document.body.appendChild(frame);
        }
    });
}

async function withPopup(resourceFile, crossOrigin, windowName)
{
    return new Promise((resolve) => { 
        let resourceURL = crossOrigin ? get_host_info().HTTP_REMOTE_ORIGIN : get_host_info().HTTP_ORIGIN;
        resourceURL += RESOURCES_DIR;
        resourceURL += resourceFile;

        let w = open(resourceURL, windowName);
        if (crossOrigin) {
            waitForCrossOriginLoad(w).then(() => {
                resolve({ 'window': w });
            });
        } else {
            w.onload = function() { resolve({ 'window': w }); };
        }
   });
}

const crossOriginPropertyNames = [ 'blur', 'close', 'closed', 'focus', 'frames', 'length', 'location', 'opener', 'parent', 'postMessage', 'self', 'top', 'window' ];
const forbiddenPropertiesCrossOrigin = ["name", "document", "history", "locationbar", "status", "frameElement", "navigator", "alert", "localStorage", "sessionStorage", "event", "foo", "bar"];

function assert_not_throwing(f, message)
{
    try {
        f();
    } catch (e) {
        assert_unreached(message);
    }
}

function checkCrossOriginPropertiesAccess(w)
{
    for (let crossOriginPropertyName of crossOriginPropertyNames)
       assert_not_throwing(function() { w[crossOriginPropertyName]; }, "Accessing property '" + crossOriginPropertyName +"' on Window should not throw");
   
    assert_false(w.closed, "'closed' property value"); 
    assert_equals(w.frames, w, "'frames' property value");
    assert_equals(w.self, w, "'self' property value");
    assert_equals(w.window, w, "'window' property value");

    assert_not_throwing(function() { w.blur(); }, "Calling blur() on Window should should throw");
    assert_not_throwing(function() { w.focus(); }, "Calling focus() on Window should should throw");
    assert_not_throwing(function() { w.postMessage('test', '*'); }, "Calling postMessage() on Window should should throw");
}

function testCrossOriginOption(w, headerValue, isCrossOrigin)
{
    if (!isCrossOrigin) {
        checkCrossOriginPropertiesAccess(w);
        for (let forbiddenPropertyCrossOrigin of forbiddenPropertiesCrossOrigin)
            assert_not_throwing(function() { eval("w." + forbiddenPropertyCrossOrigin); }, "Accessing property '" + forbiddenPropertyCrossOrigin + "' on Window should not throw");
        assert_not_throwing(function() { w.foo = 1; }, "Setting expando property should not throw");
        assert_equals(w.foo, 1, "expando property value");
        return;
    }

    // Cross-origin case.
    for (let forbiddenPropertyCrossOrigin of forbiddenPropertiesCrossOrigin) {
        assert_throws("SecurityError", function() { eval("w." + forbiddenPropertyCrossOrigin); }, "Accessing property '" + forbiddenPropertyCrossOrigin + "' on Window should throw");
        let desc = Object.getOwnPropertyDescriptor(window, forbiddenPropertyCrossOrigin);
        if (desc && desc.value)
            assert_throws("SecurityError", function() { desc.value.call(w); }, "Calling function '" + forbiddenPropertyCrossOrigin + "' on Window should throw (using getter from other window)");
        else if (desc && desc.get)
            assert_throws("SecurityError", function() { desc.get.call(w); }, "Accessing property '" + forbiddenPropertyCrossOrigin + "' on Window should throw (using getter from other window)");
    }
    assert_throws("SecurityError", function() { w.foo = 1; }, "Setting an expando property should throw");

    if (headerValue == "deny" || headerValue == "allow-postmessage") {
        for (let crossOriginPropertyName of crossOriginPropertyNames) {
            if (headerValue == "allow-postmessage" && crossOriginPropertyName == "postMessage") {
                assert_not_throwing(function() { w[crossOriginPropertyName]; }, "Accessing property '" + crossOriginPropertyName +"' on Window should not throw");
            } else {
                assert_throws("SecurityError", function() { w[crossOriginPropertyName]; }, "Accessing '" + crossOriginPropertyName + "' property");

                let desc = Object.getOwnPropertyDescriptor(window, crossOriginPropertyName);
                if (desc && desc.value)
                    assert_throws("SecurityError", function() { desc.value.call(w); }, "Calling function '" + crossOriginPropertyName + "' on Window should throw (using getter from other window)");
                else if (desc && desc.get)
                    assert_throws("SecurityError", function() { desc.get.call(w); }, "Accessing property '" + crossOriginPropertyName + "' on Window should throw (using getter from other window)");
            }
        }
        if (headerValue == "allow-postmessage") {
            assert_not_throwing(function() { w.postMessage('test', '*'); }, "Calling postMessage() on Window should not throw");
            assert_not_throwing(function() { Object.getOwnPropertyDescriptor(window, 'postMessage').value.call(w, 'test', '*'); }, "Calling postMessage() on Window should not throw (using getter from other window)");
        }

        assert_array_equals(Object.getOwnPropertyNames(w).sort(), headerValue == "allow-postmessage" ? ['postMessage'] : [], "Object.getOwnPropertyNames()");

        return;
    }

    assert_array_equals(Object.getOwnPropertyNames(w).sort(), crossOriginPropertyNames.sort(), "Object.getOwnPropertyNames()");
    checkCrossOriginPropertiesAccess(w);
}
