'use strict';

const assert = require('assert');
global.LazilyEvaluatedFunction = require('../public/v3/lazily-evaluated-function.js').LazilyEvaluatedFunction;
global.CommonComponentBase = require('../public/shared/common-component-base.js').CommonComponentBase;
const MarkupComponentBase = require('../tools/js/markup-component.js').MarkupComponentBase;

describe('MarkupComponentBase', function () {
    beforeEach(() => {
        MarkupComponentBase.reset();
    });

    describe('constructor', function () {
        it('should construct a component', () => {
            class SomeComponent extends MarkupComponentBase { };
            MarkupComponentBase.defineElement('some-component', SomeComponent);
            const component = new SomeComponent;
            assert.ok(component instanceof SomeComponent);
        });

        it('should throw if the component had not been defined', () => {
            class SomeComponent extends MarkupComponentBase { };
            assert.throws(() => new SomeComponent);
        });

        it('should throw if the component was defined with a different class (legacy named-based lookup should not be supported)', () => {
            class SomeComponent extends MarkupComponentBase {
                constructor() { super('some-component'); }
            };
            MarkupComponentBase.defineElement('some-component', SomeComponent);
            class OtherComponent extends MarkupComponentBase {
                constructor() { super('some-component'); }
            };
            assert.throws(() => new OtherComponent);
        });
    });

    describe('defineElement', function () {
        it('should throw if the component had already been defined', () => {
            class SomeComponent extends MarkupComponentBase { };
            MarkupComponentBase.defineElement('some-component', SomeComponent);
            assert.throws(() => MarkupComponentBase.defineElement('some-component', SomeComponent));
        });

        it('should throw if the same class has already been used to define another component', () => {
            class SomeComponent extends MarkupComponentBase { };
            MarkupComponentBase.defineElement('some-component', SomeComponent);
            assert.throws(() => MarkupComponentBase.defineElement('other-component', SomeComponent));
        });
    });

    describe('element', function () {
        it('should return a MarkupElement', () => {
            class SomeComponent extends MarkupComponentBase { };
            MarkupComponentBase.defineElement('some-component', SomeComponent);
            const component = new SomeComponent;
            const element = component.element();
            assert.ok(element);
            assert.equal(element.__proto__.constructor.name, 'MarkupElement');
        });

        it('should return the same element each time called', () => {
            class SomeComponent extends MarkupComponentBase { };
            MarkupComponentBase.defineElement('some-component', SomeComponent);
            const component = new SomeComponent;
            const element = component.element();
            assert.equal(component.element(), element);
        });

        it('should return a different element for each instance', () => {
            class SomeComponent extends MarkupComponentBase { };
            MarkupComponentBase.defineElement('some-component', SomeComponent);
            const component1 = new SomeComponent;
            const component2 = new SomeComponent;
            assert.notEqual(component1.element(), component2.element());
        });
    });

    describe('content', function () {
        it('should parse the content template once', () => {
            class SomeComponent extends MarkupComponentBase { };
            SomeComponent.contentTemplate = ['span', {'id': 'some'}, 'original'];
            MarkupComponentBase.defineElement('some-component', SomeComponent);
            const instance1 = new SomeComponent;
            assert.equal(instance1.content('some').textContent, 'original');
            SomeComponent.contentTemplate = ['span', {'id': 'some'}, 'modified'];
            const instance2 = new SomeComponent;
            assert.equal(instance2.content('some').textContent, 'original');
        });

        it('should upgrade components in the content tree', () => {
            class SomeComponent extends MarkupComponentBase { };
            SomeComponent.contentTemplate = ['span', ['other-component', {'id': 'other'}, 'hello']];
            MarkupComponentBase.defineElement('some-component', SomeComponent);
            class OtherComponent extends MarkupComponentBase { };
            MarkupComponentBase.defineElement('other-component', OtherComponent);
            const someComponent = new SomeComponent;
            const otherComponent = someComponent.content('other');
            assert.equal(otherComponent.localName, 'other-component');
            assert.equal(otherComponent.textContent, 'hello');
            assert.ok(otherComponent.component() instanceof OtherComponent);
        });

        it('should upgrade components in the content tree in each instance', () => {
            class SomeComponent extends MarkupComponentBase { };
            SomeComponent.contentTemplate = ['span', ['other-component', {'id': 'other'}, 'hello']];
            MarkupComponentBase.defineElement('some-component', SomeComponent);
            let constructorCount = 0;
            class OtherComponent extends MarkupComponentBase {
                constructor(...args)
                {
                    super(...args);
                    constructorCount++;
                }
            };
            MarkupComponentBase.defineElement('other-component', OtherComponent);
            assert.equal(constructorCount, 0);
            const someComponent1 = new SomeComponent;
            assert.ok(someComponent1.content('other').component() instanceof OtherComponent);
            assert.equal(constructorCount, 1);
            assert.ok(someComponent1.content('other').component() instanceof OtherComponent);
            const someComponent2 = new SomeComponent;
            assert.equal(constructorCount, 1);
            assert.ok(someComponent2.content('other').component() instanceof OtherComponent);
            assert.equal(constructorCount, 2);
        });

        it('should throw when the style template contains an unsupported selector', () => {
            class SomeComponent extends MarkupComponentBase { };
            SomeComponent.styleTemplate = {'div.target': {'font-weight': 'bold'}};
            MarkupComponentBase.defineElement('some-component', SomeComponent);
            const component = new SomeComponent;
            assert.throws(() => component.content());
        });

        describe('without arguments', function () {
            it('should return null when there are no templates', () => {
                class SomeComponent extends MarkupComponentBase { };
                MarkupComponentBase.defineElement('some-component', SomeComponent);
                const component = new SomeComponent;
                assert.equal(component.content(), null);
            });

            it('should return a MarkupContentRoot when there is a content template', () => {
                class SomeComponent extends MarkupComponentBase {
                    static get contentTemplate() { return []; }
                };
                MarkupComponentBase.defineElement('some-component', SomeComponent);
                const component = new SomeComponent;
                const contentRoot = component.content();
                assert.ok(contentRoot);
                assert.equal(contentRoot.__proto__.constructor.name, 'MarkupContentRoot');
                assert.deepEqual(contentRoot.childNodes, []);
            });
        });

        describe('with an ID', () => {
            it('should return null when there are no templates', () => {
                class SomeComponent extends MarkupComponentBase { };
                MarkupComponentBase.defineElement('some-component', SomeComponent);
                const component = new SomeComponent;
                assert.equal(component.content('some'), null);
            });

            it('should return null when there is a content template but no matching element', () => {
                class SomeComponent extends MarkupComponentBase {
                    static get contentTemplate() { return ['span', {'id': 'other'}, 'hello world']; }
                };
                MarkupComponentBase.defineElement('some-component', SomeComponent);
                const component = new SomeComponent;
                const contentRoot = component.content();
                assert.ok(contentRoot);
                assert.equal(component.content('some'), null);
            });

            it('should return the matching element when there is one', () => {
                class SomeComponent extends MarkupComponentBase {
                    static get contentTemplate() { return ['span', {'id': 'some'}, 'hello world']; }
                };
                MarkupComponentBase.defineElement('some-component', SomeComponent);
                const component = new SomeComponent;
                const contentRoot = component.content();
                assert.ok(contentRoot);
                const element = component.content('some');
                assert.ok(element);
                assert.equal(element.__proto__.constructor.name, 'MarkupElement');
                assert.equal(element.id, 'some');
                assert.equal(element.localName, 'span');
                assert.equal(element.textContent, 'hello world');
            });

            it('should return the first matching element in the tree order', () => {
                class SomeComponent extends MarkupComponentBase {
                    static get contentTemplate() { return [
                        ['div', ['b', {'id': 'some'}, 'hello']],
                        ['span', {'id': 'some'}, 'world'],
                    ]; }
                };
                MarkupComponentBase.defineElement('some-component', SomeComponent);
                const component = new SomeComponent;
                const contentRoot = component.content();
                assert.ok(contentRoot);
                const element = component.content('some');
                assert.ok(element);
                assert.equal(element.__proto__.constructor.name, 'MarkupElement');
                assert.equal(element.id, 'some');
                assert.equal(element.localName, 'b');
                assert.equal(element.textContent, 'hello');
            });
        });
    });

    describe('enqueueRender', function () {
        it('should enqueue the component to render', () => {
            let renderCalls = 0;
            class SomeComponent extends MarkupComponentBase {
                render() { renderCalls++; }
            };
            MarkupComponentBase.defineElement('some-component', SomeComponent);
            const component = new SomeComponent;
            component.enqueueToRender();
            assert.equal(renderCalls, 0);
            MarkupComponentBase.runRenderLoop();
            assert.equal(renderCalls, 1);
        });

        it('should not enqueue the same component multiple times', () => {
            let renderCalls = 0;
            class SomeComponent extends MarkupComponentBase {
                render() { renderCalls++; }
            };
            MarkupComponentBase.defineElement('some-component', SomeComponent);
            const component = new SomeComponent;
            component.enqueueToRender();
            component.enqueueToRender();
            assert.equal(renderCalls, 0);
            MarkupComponentBase.runRenderLoop();
            assert.equal(renderCalls, 1);
        });
    });

    describe('runRenderLoop', function () {
        it('should invoke render() on enqueued components in the oreder', () => {
            let renderCalls = [];
            class SomeComponent extends MarkupComponentBase {
                render() { renderCalls.push(this); }
            };
            MarkupComponentBase.defineElement('some-component', SomeComponent);
            const component1 = new SomeComponent;
            const component2 = new SomeComponent;
            component1.enqueueToRender();
            component2.enqueueToRender();
            assert.deepEqual(renderCalls, []);
            MarkupComponentBase.runRenderLoop();
            assert.deepEqual(renderCalls, [component1, component2]);
        });

        it('should process cascading calls to enqueueRender()', () => {
            let renderCalls = [];
            class SomeComponent extends MarkupComponentBase {
                render() {
                    renderCalls.push(this);
                    if (this == instance1)
                        instance2.enqueueToRender();
                }
            };
            MarkupComponentBase.defineElement('some-component', SomeComponent);
            const instance1 = new SomeComponent;
            const instance2 = new SomeComponent;
            instance1.enqueueToRender();
            assert.deepEqual(renderCalls, []);
            MarkupComponentBase.runRenderLoop();
            assert.deepEqual(renderCalls, [instance1, instance2]);
        });

        it('should delay render() call upon a cascading enqueuing', () => {
            let renderCalls = [];
            class SomeComponent extends MarkupComponentBase {
                render() {
                    renderCalls.push(this);
                    if (this == instance1)
                        instance2.enqueueToRender();
                }
            };
            MarkupComponentBase.defineElement('some-component', SomeComponent);
            const instance1 = new SomeComponent;
            const instance2 = new SomeComponent;
            instance1.enqueueToRender();
            instance2.enqueueToRender();
            assert.deepEqual(renderCalls, []);
            MarkupComponentBase.runRenderLoop();
            assert.deepEqual(renderCalls, [instance1, instance2]);
        });

        it('should call render() again when a cascading enqueueing occurs after the initial call', () => {
            let renderCalls = [];
            class SomeComponent extends MarkupComponentBase {
                render() {
                    renderCalls.push(this);
                    if (this == instance1)
                        instance2.enqueueToRender();
                }
            };
            MarkupComponentBase.defineElement('some-component', SomeComponent);
            const instance1 = new SomeComponent;
            const instance2 = new SomeComponent;
            instance2.enqueueToRender();
            instance1.enqueueToRender();
            assert.deepEqual(renderCalls, []);
            MarkupComponentBase.runRenderLoop();
            assert.deepEqual(renderCalls, [instance1, instance2, instance1]);
        });
    });

    describe('renderReplace', function () {
        it('should remove old children', () => {
            class SomeComponent extends MarkupComponentBase {
                render() {
                    const element = MarkupComponentBase.createElement;
                    this.renderReplace(this.content(), element('b', 'world'));
                }

                static get contentTemplate() {
                    return ['span', 'hello'];
                }
            };
            MarkupComponentBase.defineElement('some-component', SomeComponent);
            const instance = new SomeComponent;

            let content = instance.content();
            assert.equal(content.childNodes.length, 1);
            assert.equal(content.childNodes[0].localName, 'span');
            assert.equal(content.childNodes[0].textContent, 'hello');

            instance.enqueueToRender();
            MarkupComponentBase.runRenderLoop();

            content = instance.content();
            assert.equal(content.childNodes.length, 1);
            assert.equal(content.childNodes[0].localName, 'b');
            assert.equal(content.childNodes[0].textContent, 'world');
        });

        it('should insert the element of a component in the content tree', () => {
            class SomeComponent extends MarkupComponentBase {
                render() {
                    this.renderReplace(this.content(), new OtherComponent);
                }
                static get contentTemplate() {
                    return [];
                }
            };
            MarkupComponentBase.defineElement('some-component', SomeComponent);

            class OtherComponent extends MarkupComponentBase { };
            MarkupComponentBase.defineElement('other-component', OtherComponent);

            const someComponent = new SomeComponent;
            const content = someComponent.content();
            assert.equal(content.childNodes.length, 0);

            someComponent.enqueueToRender();
            MarkupComponentBase.runRenderLoop();

            assert.equal(content.childNodes.length, 1);
            assert.equal(content.childNodes[0].localName, 'other-component');
            assert.equal(content.childNodes[0].textContent, '');
            const otherComponent = content.childNodes[0].component();
            assert.ok(otherComponent instanceof OtherComponent);
        });

        it('should add classes to the generated elements if there are matching styles', () => {
            class SomeComponent extends MarkupComponentBase {
                render() {
                    this.renderReplace(this.content(), [
                        this.createElement('div'),
                        this.createElement('section', {class: 'target'}),
                    ]);
                }
            };
            SomeComponent.styleTemplate = {
                'div': {'font-weight': 'bold'},
                '.target': {'border': 'solid 1px blue'},
            }
            MarkupComponentBase.defineElement('some-component', SomeComponent);
            const instance = new SomeComponent;
            instance.enqueueToRender();
            MarkupComponentBase.runRenderLoop();

            const content = instance.content();
            const div = content.childNodes[0];
            const section = content.childNodes[1];
            assert.equal(div.localName, 'div');
            assert.ok(div.getAttribute('class'));
            assert.equal(section.localName, 'section');
            assert.ok(section.getAttribute('class').split(/\s+/).length, 2);
        });
    });

    describe('createElement', function () {

        it('should create an element of the specified name', () => {
            const div = MarkupComponentBase.createElement('div');
            assert.equal(div.localName, 'div');
            assert.equal(div.__proto__.constructor.name, 'MarkupElement');
        });

        it('should create an element with the specified attributes', () => {
            const input = MarkupComponentBase.createElement('input', {'title': 'hi', 'id': 'foo', 'required': false, 'checked': true});
            assert.equal(input.localName, 'input');
            assert.equal(input.attributes.length, 3);
            assert.equal(input.attributes[0].localName, 'title');
            assert.equal(input.attributes[0].value, 'hi');
            assert.equal(input.attributes[1].localName, 'id');
            assert.equal(input.attributes[1].value, 'foo');
            assert.equal(input.attributes[2].localName, 'checked');
            assert.equal(input.attributes[2].value, '');
        });

        it('should throw when an event handler is set', () => {
            assert.throws(() => MarkupComponentBase.createElement('a', {'onclick': () => {}}));
        });

        it('should create an element with the specified children when the second argument is a span', () => {
            const element = MarkupComponentBase.createElement;
            const span = element('span');
            const div = element('div', span);
            assert.equal(div.attributes.length, 0);
            assert.equal(div.childNodes.length, 1);
            assert.equal(div.childNodes[0], span);
        });

        it('should create an element with the specified children when the second argument is a string', () => {
            const element = MarkupComponentBase.createElement;
            const div = element('div', 'hello');
            assert.equal(div.attributes.length, 0);
            assert.equal(div.childNodes.length, 1);
            assert.equal(div.childNodes[0].__proto__.constructor.name, 'MarkupText');
            assert.equal(div.childNodes[0].data, 'hello');
        });

        it('should create an element with the specified children when the second argument is a component', () => {
            class SomeComponent extends MarkupComponentBase { };
            MarkupComponentBase.defineElement('some-component', SomeComponent);
            const element = MarkupComponentBase.createElement;
            const component = new SomeComponent;
            const div = element('div', component);
            assert.equal(div.attributes.length, 0);
            assert.equal(div.childNodes.length, 1);
            assert.equal(div.childNodes[0], component.element());
        });

        it('should create an element with the specified attributes and children', () => {
            const element = MarkupComponentBase.createElement;
            const span = element('span');
            const div = element('div', {'lang': 'en'}, [span, 'hi']);
            assert.equal(div.localName, 'div');
            assert.equal(div.attributes.length, 1);
            assert.equal(div.attributes[0].localName, 'lang');
            assert.equal(div.attributes[0].value, 'en');
            assert.equal(div.childNodes.length, 2);
            assert.equal(div.childNodes[0], span);
            assert.equal(div.childNodes[1].data, 'hi');
        });
    });

    describe('createLink', function () {
        it('should create an anchor element', () => {
            const anchor = MarkupComponentBase.createLink('hello', '#some-url');
            assert.equal(anchor.localName, 'a');
            assert.equal(anchor.__proto__.constructor.name, 'MarkupElement');
        });

        it('should create an anchor element with href and title when the second argument is a string and the third argument is ommitted', () => {
            const anchor = MarkupComponentBase.createLink('hello', '#some-url');
            assert.equal(anchor.localName, 'a');
            assert.equal(anchor.__proto__.constructor.name, 'MarkupElement');
            assert.equal(anchor.attributes.length, 2);
            assert.equal(anchor.getAttribute('href'), '#some-url');
            assert.equal(anchor.getAttribute('title'), 'hello');
            assert.equal(anchor.textContent, 'hello');
        });

        it('should create an anchor element with href and title when the second and third arguments are string', () => {
            const anchor = MarkupComponentBase.createLink('hello', 'some link', '#some-url');
            assert.equal(anchor.localName, 'a');
            assert.equal(anchor.__proto__.constructor.name, 'MarkupElement');
            assert.equal(anchor.attributes.length, 2);
            assert.equal(anchor.getAttribute('href'), '#some-url');
            assert.equal(anchor.getAttribute('title'), 'some link');
            assert.equal(anchor.textContent, 'hello');
        });

        it('should throw when the second argument is a function', () => {
            assert.throws(() => MarkupComponentBase.createLink('hello', () => { }));
        });

        it('should throw when the third argument is a function', () => {
            assert.throws(() => MarkupComponentBase.createLink('hello', 'some link', () => { }));
        });

        it('should create an anchor element with target=_blank when isExternal is true', () => {
            const anchor = MarkupComponentBase.createLink('hello', 'some link', '#some-url', true);
            assert.equal(anchor.localName, 'a');
            assert.equal(anchor.__proto__.constructor.name, 'MarkupElement');
            assert.equal(anchor.attributes.length, 3);
            assert.equal(anchor.getAttribute('href'), '#some-url');
            assert.equal(anchor.getAttribute('title'), 'some link');
            assert.equal(anchor.getAttribute('target'), '_blank');
            assert.equal(anchor.textContent, 'hello');
        });
    });
   
});
