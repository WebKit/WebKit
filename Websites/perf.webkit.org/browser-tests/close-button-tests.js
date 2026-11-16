
describe('CloseButton', () => {
    const scripts = ['instrumentation.js', '../shared/common-component-base.js', 'components/base.js', 'components/button-base.js', 'components/close-button.js'];

    it('must dispatch "activate" action when the anchor is clicked', () => {
        const context = new BrowsingContext();
        return context.importScripts(scripts, 'CloseButton').then((CloseButton) => {
            const closeButton = new CloseButton;
            context.document.body.appendChild(closeButton.element());

            closeButton.content().querySelector('a').click();

            let activateCount = 0;
            closeButton.listenToAction('activate', () => {
                activateCount++;
            });
            expect(activateCount).to.be(0);
            closeButton.content().querySelector('a').click();
            expect(activateCount).to.be(1);
            closeButton.content().querySelector('a').click();
            expect(activateCount).to.be(2);
        });
    });

});
