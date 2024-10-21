browser.runtime.onMessage.addListener(async (message, sender, sendResponse) => {
    browser.test.assertEq(message?.action, 'openPopup', 'Message action should be openPopup');

    browser.action.openPopup();

    sendResponse({ success: true });
});

browser.test.yield('Load Tab');
