'use strict';

const assert = require('assert');
global.LazilyEvaluatedFunction = require('../public/v3/lazily-evaluated-function.js').LazilyEvaluatedFunction;
global.CommonComponentBase = require('../public/shared/common-component-base.js').CommonComponentBase;
const {MarkupComponentBase, MarkupPage} = require('../tools/js/markup-component.js');

describe('MarkupPage', function () {
    beforeEach(() => {
        MarkupComponentBase.reset();
    });

    describe('generateMarkup', function () {
        it('should render page contents', () => {
            class SomePage extends MarkupPage { }
            SomePage.pageContent = ['div', 'hello, world'];
            MarkupComponentBase.defineElement('some-page', SomePage);
            const page = new SomePage;
            assert.ok(page instanceof SomePage);
            const markup = page.generateMarkup();
            assert.ok(markup.startsWith('<!DOCTYPE html><html><head'));
            assert.ok(markup.includes('</head><body'));
            assert.ok(markup.endsWith('</body></html>'));
            assert.ok(markup.includes('<div>hello, world</div>'));
        });

        it('should render page contents with stylesheet when a style template is available', () => {
            class SomePage extends MarkupPage { }
            SomePage.pageContent = ['div', {class: 'container'}, 'hello, world'];
            SomePage.styleTemplate = {'.container': {'font-weight': 'bold'}};
            MarkupComponentBase.defineElement('some-page', SomePage);
            const page = new SomePage;
            assert.ok(page instanceof SomePage);
            const markup = page.generateMarkup();
            assert.ok(markup.search(/font-weight\:\s*bold;\s*}\s*<\/style>/));
        });
    });
   
});
