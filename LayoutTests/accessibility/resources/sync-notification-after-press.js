// Shared by sync-notification-after-press.html and isolated-tree/sync-notification-after-press.html.
// The page needs an input with id "field" and a button with id "button" whose click handler changes the field's value.

var output = "Tests that AXSyncNotification requested after AXPress is posted after the notifications for the DOM changes the click handler made.\n\n";

// Notifications in the order they were received.
var receivedNotifications = [];
var syncNotificationReceived;
var fieldValueAtSync;

function notificationCallback(element, notificationName, userInfo) {
    if (notificationName === "AXSyncNotification" && userInfo?.AXSyncNotificationHandle === "after-press") {
        receivedNotifications.push({ name: notificationName });
        fieldValueAtSync = axField.stringValue;
        syncNotificationReceived();
    } else if (notificationName === "AXValueChanged")
        receivedNotifications.push({ name: notificationName, id: element.domIdentifier });
}

function fieldValueChangesBeforeSync() {
    const syncIndex = receivedNotifications.findIndex((notification) => notification.name === "AXSyncNotification");
    return receivedNotifications.slice(0, syncIndex).filter((notification) => notification.name === "AXValueChanged" && notification.id === "field").length;
}

var axField;
var axButton;
if (window.accessibilityController) {
    window.jsTestIsAsync = true;

    axField = accessibilityController.accessibleElementById("field");
    axButton = accessibilityController.accessibleElementById("button");

    setTimeout(async function() {
        output += expect("accessibilityController.addNotificationListener(notificationCallback)", "true");

        const received = new Promise((resolve) => { syncNotificationReceived = resolve; });
        // AXPress is performed asynchronously, so the sync notification must still wait for the click handler.
        axButton.press();
        output += expect("axButton.postSyncNotification('after-press')", "true");
        await received;

        output += expect("document.getElementById('field').value", "'pressed'");
        output += expect("fieldValueChangesBeforeSync() > 0", "true");
        output += expect("fieldValueAtSync", "'AXValue: pressed'");

        accessibilityController.removeNotificationListener();
        debug(output);
        finishJSTest();
    }, 0);
}
