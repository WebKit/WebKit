
describe('Page', function() {

    describe('open', () => {
        it('must replace the content of document.body', async () => {
            const context = new BrowsingContext();
            const Page = await context.importScripts(['instrumentation.js', '../shared/common-component-base.js', 'components/base.js', 'pages/page.js'], 'Page');

            class SomePage extends Page {
                constructor() { super('some page'); }
            };

            const somePage = new SomePage;

            const document = context.document;
            document.body.innerHTML = '<p>hello, world</p>';
            expect(document.querySelector('p')).not.to.be(null);
            expect(document.querySelector('page-component')).to.be(null);

            somePage.open();

            expect(document.querySelector('p')).to.be(null);
            expect(document.querySelector('page-component')).to.be(somePage.element());
        });

        it('must update the document title', async () => {
            const context = new BrowsingContext();
            const Page = await context.importScripts(['instrumentation.js', '../shared/common-component-base.js', 'components/base.js', 'pages/page.js'], 'Page');

            class SomePage extends Page {
                constructor() { super('some page'); }
            };

            const somePage = new SomePage;

            const document = context.document;
            expect(document.title).to.be('');
            somePage.open();
            expect(document.title).to.be('some page');
        });

        it('must enqueue itself to render', async () => {
            const context = new BrowsingContext();
            const [Page, ComponentBase] = await context.importScripts(['instrumentation.js', '../shared/common-component-base.js', 'components/base.js', 'pages/page.js'], 'Page', 'ComponentBase');

            let renderCount = 0;
            class SomePage extends Page {
                constructor() { super('some page'); }
                render() { renderCount++; }
            };

            expect(renderCount).to.be(0);
            const somePage = new SomePage;
            await waitForComponentsToRender(context);
            expect(renderCount).to.be(0);

            somePage.open();
            await waitForComponentsToRender(context);
            expect(renderCount).to.be(1);
        });

        it('must update the current page of the router', async () => {
            const context = new BrowsingContext();
            const [Page, PageRouter, ComponentBase] = await context.importScripts(
                ['instrumentation.js', '../shared/common-component-base.js', 'components/base.js', 'pages/page.js', 'pages/page-router.js'],
                'Page', 'PageRouter', 'ComponentBase');

            class SomePage extends Page {
                constructor() { super('some page'); }
                routeName() { return 'some-page'; }
            };

            const router = new PageRouter;
            const somePage = new SomePage;
            router.addPage(somePage);
            expect(router.currentPage()).to.be(null);
            somePage.open();
            expect(router.currentPage()).to.be(somePage);
        });
    });

    describe('enqueueToRender', () => {
        it('must not enqueue itself to render if the router is set and the current page is not itself', async () => {
            const context = new BrowsingContext();
            const [Page, PageRouter, ComponentBase] = await context.importScripts(
                ['instrumentation.js', '../shared/common-component-base.js', 'components/base.js', 'pages/page.js', 'pages/page-router.js'],
                'Page', 'PageRouter', 'ComponentBase');

            let someRenderCount = 0;
            class SomePage extends Page {
                constructor() { super('some page'); }
                render() { someRenderCount++; }
                routeName() { return 'some-page'; }
            };

            let anotherRenderCount = 0;
            class AnotherPage extends Page {
                constructor() { super('another page'); }
                render() { anotherRenderCount++; }
                routeName() { return 'another-page'; }
            };

            const router = new PageRouter;
            const somePage = new SomePage;
            const anotherPage = new AnotherPage;
            router.addPage(somePage);
            router.addPage(anotherPage);
            router.setDefaultPage(somePage);
            router.route();
            await waitForComponentsToRender(context);
            expect(someRenderCount).to.be(1);
            expect(anotherRenderCount).to.be(0);

            anotherPage.open();
            await waitForComponentsToRender(context);
            expect(someRenderCount).to.be(1);
            expect(anotherRenderCount).to.be(1);

            somePage.enqueueToRender();
            anotherPage.enqueueToRender();
            await waitForComponentsToRender(context);
            expect(someRenderCount).to.be(1);
            expect(anotherRenderCount).to.be(2);
        });
    });

});
