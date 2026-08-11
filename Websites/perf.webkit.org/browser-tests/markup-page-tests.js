
describe('MarkupPage', function () {

    async function importMarkupComponent(context)
    {
        return await context.importScripts(['lazily-evaluated-function.js', '../shared/common-component-base.js', '../../tools/js/markup-component.js'],
            'MarkupComponentBase', 'MarkupPage');
    }

    describe('pageContent', function () {
        it('should define the content of the generated page', async () => {
            const context = new BrowsingContext();
            const [MarkupComponentBase, MarkupPage] = await importMarkupComponent(context);

            class SomePage extends MarkupPage { };
            SomePage.pageContent = ['div', 'hello world'];
            MarkupComponentBase.defineElement('some-page', SomePage);

            const page = new SomePage;
            expect(context.document.title).not.to.be('Some Page');
            expect(page.generateMarkup()).to.contain('<div>hello world</div>');
        });
    });

    describe('content', function () {
        it('should return the page body', async () => {
            const context = new BrowsingContext();
            const [MarkupComponentBase, MarkupPage] = await importMarkupComponent(context);

            class SomePage extends MarkupPage {
                render()
                {
                    super.render();
                    this.renderReplace(this.content(), MarkupComponentBase.createElement('span'));
                }
            }
            SomePage.pageContent = ['div', ['some-component']];
            SomePage.styleTemplate = {'span': {'font-weight': 'bold'}};
            MarkupComponentBase.defineElement('some-page', SomePage);

            const page = new SomePage;
            context.document.open();
            context.document.write(page.generateMarkup());
            context.document.close();
            expect(context.document.head.querySelector('style')).to.be.a(context.global.HTMLStyleElement);
            expect(context.document.head.querySelector('title')).to.be.a(context.global.HTMLTitleElement);
        });
    });

    describe('generateMarkup', function () { 
        it('must enqueue itself to render and run the render loop', async () => {
            const context = new BrowsingContext();
            const [MarkupComponentBase, MarkupPage] = await importMarkupComponent(context);
            let renderCall = 0;
            class SomePage extends MarkupPage {
                render() {
                    super.render();
                    renderCall++;
                }
            }
            MarkupComponentBase.defineElement('some-page', SomePage);
            const page = new SomePage;
            page.generateMarkup();
            expect(renderCall).to.be.greaterThan(0);
        });

        it('must generate DOCTYPE, html, head, and body elements', async () => {
            const context = new BrowsingContext();
            const [MarkupComponentBase, MarkupPage] = await importMarkupComponent(context);

            let renderCall = 0;
            class SomePage extends MarkupPage {
                render() {
                    super.render();
                    renderCall++;
                }
            }
            MarkupComponentBase.defineElement('some-page', SomePage);
            const page = new SomePage;
            expect(page.generateMarkup()).to.contain('<!DOCTYPE html><html><head');
            expect(page.generateMarkup()).to.contain('</head><body');
        });

        it('must generate the title element', async () => {
            const context = new BrowsingContext();
            const [MarkupComponentBase, MarkupPage] = await importMarkupComponent(context);

            let renderCall = 0;
            class SomePage extends MarkupPage {
                constructor() { super('Some Page'); }
                render() {
                    super.render();
                    renderCall++;
                }
            }
            MarkupComponentBase.defineElement('some-page', SomePage);
            const page = new SomePage;
            expect(context.document.title).not.to.be('Some Page');
            context.document.open();
            context.document.write(page.generateMarkup());
            context.document.close();
            expect(context.document.title).to.be('Some Page');
        });

        it('must generate the content for components in the page', async () => {
            const context = new BrowsingContext();
            const [MarkupComponentBase, MarkupPage] = await importMarkupComponent(context);

            class SomePage extends MarkupPage { }
            SomePage.pageContent = ['div', ['some-component']];
            MarkupComponentBase.defineElement('some-page', SomePage);

            class SomeComponent extends MarkupComponentBase { };
            SomeComponent.contentTemplate = ['div', 'hello world'];
            MarkupComponentBase.defineElement('some-component', SomeComponent);

            const page = new SomePage;
            expect(page.generateMarkup()).to.contain('<div>hello world</div>');
        });

        it('must generate the style for components in the page', async () => {
            const context = new BrowsingContext();
            const [MarkupComponentBase, MarkupPage] = await importMarkupComponent(context);

            class SomePage extends MarkupPage { }
            SomePage.pageContent = ['div', ['some-component']];
            MarkupComponentBase.defineElement('some-page', SomePage);

            class SomeComponent extends MarkupComponentBase {};
            SomeComponent.contentTemplate = [['span', 'hello world'], ['p']];
            SomeComponent.styleTemplate = {'span': {'font-weight': 'bold'}};
            MarkupComponentBase.defineElement('some-component', SomeComponent);

            const page = new SomePage;
            context.document.open();
            context.document.write(page.generateMarkup());
            context.document.close();
            expect(context.global.getComputedStyle(context.document.querySelector('p')).fontWeight).to.be('normal');
            expect(context.global.getComputedStyle(context.document.querySelector('span')).fontWeight).to.be('bold');
        });

        it('must not apply the styles from a sibling component', async () => {
            const context = new BrowsingContext();
            const [MarkupComponentBase, MarkupPage] = await importMarkupComponent(context);

            class SomePage extends MarkupPage { }
            SomePage.pageContent = ['div', [['some-component'], ['other-component']]];
            MarkupComponentBase.defineElement('some-page', SomePage);

            class SomeComponent extends MarkupComponentBase {};
            SomeComponent.contentTemplate = [['section', 'hello'], ['p', 'world']];
            SomeComponent.styleTemplate = {'section': {'font-weight': 'bold'}};
            MarkupComponentBase.defineElement('some-component', SomeComponent);

            class OtherComponent extends MarkupComponentBase {};
            OtherComponent.contentTemplate = [['section', 'hello'], ['p', 'world']];
            OtherComponent.styleTemplate = {'p': {'color': 'blue'}};
            MarkupComponentBase.defineElement('other-component', OtherComponent);

            const page = new SomePage;
            context.document.open();
            context.document.write(page.generateMarkup());
            context.document.close();
            const getComputedStyle = (element) => context.global.getComputedStyle(element);
            const querySelector = (selector) => context.document.querySelector(selector);
            expect(getComputedStyle(querySelector('some-component section')).fontWeight).to.be('bold');
            expect(getComputedStyle(querySelector('other-component section')).fontWeight).to.be('normal');
            expect(getComputedStyle(querySelector('some-component p')).color).to.be('rgb(0, 0, 0)');
            expect(getComputedStyle(querySelector('other-component p')).color).to.be('rgb(0, 0, 255)');
        });

        it('must not apply the styles from a ancestor component', async () => {
            const context = new BrowsingContext();
            const [MarkupComponentBase, MarkupPage] = await importMarkupComponent(context);

            class SomePage extends MarkupPage { }
            SomePage.pageContent = ['div', ['some-component']];
            SomePage.styleTemplate = {'div': {'width': '300px'}};
            MarkupComponentBase.defineElement('some-page', SomePage);

            class SomeComponent extends MarkupComponentBase {};
            SomeComponent.contentTemplate = ['div', ['other-component']];
            SomeComponent.styleTemplate = {'div': {'width': '200px'}};
            MarkupComponentBase.defineElement('some-component', SomeComponent);

            class OtherComponent extends MarkupComponentBase {};
            OtherComponent.contentTemplate = ['div'];
            OtherComponent.styleTemplate = {'div': {'width': '100px'}};
            MarkupComponentBase.defineElement('other-component', OtherComponent);

            const page = new SomePage;
            context.document.open();
            context.document.write(page.generateMarkup());
            context.document.close();
            const getComputedStyle = (element) => context.global.getComputedStyle(element);
            const divs = Array.from(context.document.querySelectorAll('div'));
            expect(getComputedStyle(divs[0]).width).to.be('300px');
            expect(getComputedStyle(divs[1]).width).to.be('200px');
            expect(getComputedStyle(divs[2]).width).to.be('100px');
        });

        it('must apply the styles to elements generated in renderReplace', async () => {
            const context = new BrowsingContext();
            const [MarkupComponentBase, MarkupPage] = await importMarkupComponent(context);

            class SomePage extends MarkupPage { }
            SomePage.pageContent = ['div', ['some-component']];
            MarkupComponentBase.defineElement('some-page', SomePage);

            class SomeComponent extends MarkupComponentBase {
                render()
                {
                    super.render();
                    this.renderReplace(this.content(), MarkupComponentBase.createElement('span'));
                }
            };
            SomeComponent.contentTemplate = [];
            SomeComponent.styleTemplate = {'span': {'font-weight': 'bold'}};
            MarkupComponentBase.defineElement('some-component', SomeComponent);

            const page = new SomePage;
            context.document.open();
            context.document.write(page.generateMarkup());
            context.document.close();
            expect(context.global.getComputedStyle(context.document.querySelector('span')).fontWeight).to.be('bold');
        });

        it('must apply the styles to elements based on class names', async () => {
            const context = new BrowsingContext();
            const [MarkupComponentBase, MarkupPage] = await importMarkupComponent(context);

            class SomePage extends MarkupPage { }
            SomePage.pageContent = ['div', {class: 'target'}, ['some-component']];
            SomePage.styleTemplate = {'.target': {'border': 'solid 1px black'}};
            MarkupComponentBase.defineElement('some-page', SomePage);

            const page = new SomePage;
            context.document.open();
            context.document.write(page.generateMarkup());
            context.document.close();
            const target = context.document.querySelector('.target');
            expect(context.global.getComputedStyle(target).borderWidth).to.be('1px');
            expect(target.classList.length).to.be(2);
        });

        it('must not add the same class name multiple times to an element', async () => {
            const context = new BrowsingContext();
            const [MarkupComponentBase, MarkupPage] = await importMarkupComponent(context);

            class SomePage extends MarkupPage {
                render()
                {
                    super.render();
                    const container = this.createElement('div');
                    this.renderReplace(container, this.createElement('div', {class: 'target'}));
                    this.renderReplace(this.content(), container);
                }
            }
            SomePage.pageContent = [];
            SomePage.styleTemplate = {'.target': {'border': 'solid 1px black'}};
            MarkupComponentBase.defineElement('some-page', SomePage);

            const page = new SomePage;
            context.document.open();
            context.document.write(page.generateMarkup());
            context.document.close();
            const target = context.document.querySelector('.target');
            expect(context.global.getComputedStyle(target).borderWidth).to.be('1px');
            expect(target.classList.length).to.be(2);
        });

    });
});

