'use strict';

const assert = require('assert');
global.LazilyEvaluatedFunction = require('../public/v3/lazily-evaluated-function.js').LazilyEvaluatedFunction;
global.CommonComponentBase = require('../public/shared/common-component-base.js').CommonComponentBase;
const MarkupComponentBase = require('../tools/js/markup-component.js').MarkupComponentBase;

describe('MarkupElement', function () {
    beforeEach(() => {
        MarkupComponentBase.reset();
    });

    function createElement(name)
    {
        class DummyComponent extends MarkupComponentBase { }
        DummyComponent.contentTemplate = [name];
        MarkupComponentBase.defineElement('dummy-component', DummyComponent);
        const component = new DummyComponent;
        return component.content().childNodes[0];
    }

    describe('style', function () {
        it('should set the style content attribute', () => {
            const div = createElement('div');
            assert.equal(div.getAttribute('style'), null);
            div.style.color = 'blue';
            assert.equal(div.getAttribute('style'), 'color: blue');
        });

        it('should convert camelCased property names', () => {
            const div = createElement('div');
            assert.equal(div.getAttribute('style'), null);
            div.style.fontWeight = 'bold';
            assert.equal(div.getAttribute('style'), 'font-weight: bold');
        });

        it('should be able to serialize multiple properties', () => {
            const div = createElement('div');
            assert.equal(div.getAttribute('style'), null);
            div.style.color = 'blue';
            div.style.fontWeight = 'bold';
            assert.equal(div.getAttribute('style'), 'color: blue; font-weight: bold');
        });

        it('should override properties after the conversion from camelCase', () => {
            const div = createElement('div');
            assert.equal(div.getAttribute('style'), null);
            div.style['font-weight'] = 'bold';
            assert.equal(div.getAttribute('style'), 'font-weight: bold');
            div.style.fontWeight = 'normal';
            assert.equal(div.getAttribute('style'), 'font-weight: normal');
        });
    });

    describe('setAttribute', function () {
        it('should override the inline style', () => {
            const div = createElement('div');
            assert.equal(div.getAttribute('style'), null);
            div.style.color = 'blue';
            assert.equal(div.getAttribute('style'), 'color: blue');
            div.setAttribute('style', 'font-weight: bold');
            assert.equal(div.getAttribute('style'), 'font-weight: bold');
        });
    });
   
});
