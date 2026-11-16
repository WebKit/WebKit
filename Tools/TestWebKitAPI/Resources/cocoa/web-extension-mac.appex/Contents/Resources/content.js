(async () => {
    const response = await browser.runtime.sendMessage({ action: 'openPopup' });
    browser.test.assertEq(response?.success, true, 'Popup should be opened successfully by the background script');
})();
