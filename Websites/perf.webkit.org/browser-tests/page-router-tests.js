
describe('PageRouter', () => {
    describe('route', () => {
        it('should choose the longest match', async () => {
            const context = new BrowsingContext();
            const [Page, PageRouter, ComponentBase] = await context.importScripts(
                ['instrumentation.js', '../shared/common-component-base.js', 'components/base.js', 'pages/page.js', 'pages/page-router.js'],
                'Page', 'PageRouter', 'ComponentBase');

            let someRenderCount = 0;
            class SomePage extends Page {
                constructor() { super('some page'); }
                render() { someRenderCount++; }
                routeName() { return 'page'; }
            };

            let anotherRenderCount = 0;
            class AnotherPage extends Page {
                constructor() { super('another page'); }
                render() { anotherRenderCount++; }
                routeName() { return 'page/another'; }
            };

            const router = new PageRouter;
            const somePage = new SomePage;
            const anotherPage = new AnotherPage;
            router.addPage(somePage);
            router.addPage(anotherPage);
            context.global.location.hash = '#/page/another/1';
            router.route();
            await waitForComponentsToRender(context);
            expect(someRenderCount).to.be(0);
            expect(anotherRenderCount).to.be(1);
        });
    });
});