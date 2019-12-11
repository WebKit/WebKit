
describe('ComponentBase', function() {

    async function importComponentBase(context)
    {
        const [Instrumentation, CommonComponentBase, ComponentBase] = await context.importScripts(
            ['instrumentation.js', '../shared/common-component-base.js', 'components/base.js'],
            'Instrumentation', 'CommonComponentBase', 'ComponentBase');
        return ComponentBase;
    }

    function createTestToCheckExistenceOfShadowTree(callback, options = {htmlTemplate: false, cssTemplate: true})
    {
        const context = new BrowsingContext();
        return importComponentBase(context).then((ComponentBase) => {
            class SomeComponent extends ComponentBase { }
            if (options.htmlTemplate)
                SomeComponent.htmlTemplate = () => { return '<div id="div" style="height: 10px;"></div>'; };
            if (options.cssTemplate)
                SomeComponent.cssTemplate = () => { return ':host { height: 10px; }'; };

            let instance = new SomeComponent('some-component');
            instance.element().style.display = 'block';
            context.document.body.appendChild(instance.element());
            return callback(instance, () => { return instance.element().offsetHeight == 10; });
        });
    }

    it('must enqueue a connected component to render', () => {
        const context = new BrowsingContext();
        return importComponentBase(context).then((ComponentBase) => {
            let renderCall = 0;
            class SomeComponent extends ComponentBase {
                render() { renderCall++; }
            }
            ComponentBase.defineElement('some-component', SomeComponent);

            let requestAnimationFrameCount = 0;
            let callback = null;
            context.global.requestAnimationFrame = (newCallback) => {
                callback = newCallback;
                requestAnimationFrameCount++;
            }

            expect(requestAnimationFrameCount).to.be(0);
            const instance = new SomeComponent;
            context.document.body.appendChild(instance.element());
            expect(requestAnimationFrameCount).to.be(1);
            callback();
            expect(renderCall).to.be(1);
            expect(requestAnimationFrameCount).to.be(1);
        });
    });

    it('must enqueue a connected component to render upon a resize event if enqueueToRenderOnResize is true', () => {
        const context = new BrowsingContext();
        return importComponentBase(context).then((ComponentBase) => {
            class SomeComponent extends ComponentBase {
                static get enqueueToRenderOnResize() { return true; }
            }
            ComponentBase.defineElement('some-component', SomeComponent);

            let requestAnimationFrameCount = 0;
            let callback = null;
            context.global.requestAnimationFrame = (newCallback) => {
                callback = newCallback;
                requestAnimationFrameCount++;
            }

            expect(requestAnimationFrameCount).to.be(0);
            const instance = new SomeComponent;
            context.global.dispatchEvent(new Event('resize'));
            context.document.body.appendChild(instance.element());
            context.global.dispatchEvent(new Event('resize'));
            expect(requestAnimationFrameCount).to.be(1);
        });
    });

    it('must not enqueue a disconnected component to render upon a resize event if enqueueToRenderOnResize is true', () => {
        const context = new BrowsingContext();
        return importComponentBase(context).then((ComponentBase) => {
            class SomeComponent extends ComponentBase {
                static get enqueueToRenderOnResize() { return true; }
            }
            ComponentBase.defineElement('some-component', SomeComponent);

            let requestAnimationFrameCount = 0;
            let callback = null;
            context.global.requestAnimationFrame = (newCallback) => {
                callback = newCallback;
                requestAnimationFrameCount++;
            }

            const instance = new SomeComponent;
            expect(requestAnimationFrameCount).to.be(0);
            context.global.dispatchEvent(new Event('resize'));
            expect(requestAnimationFrameCount).to.be(0);
        });
    });

    describe('constructor', () => {
        it('is a function', () => {
            return importComponentBase(new BrowsingContext()).then((ComponentBase) => {
                expect(ComponentBase).to.be.a('function');
            });
        });

        it('can be instantiated', () => {
            return importComponentBase(new BrowsingContext()).then((ComponentBase) => {
                let callCount = 0;
                class SomeComponent extends ComponentBase {
                    constructor() {
                        super('some-component');
                        callCount++;
                    }
                }
                let instance = new SomeComponent;
                expect(instance).to.be.a(ComponentBase);
                expect(instance).to.be.a(SomeComponent);
                expect(callCount).to.be(1);
            });
        });

        it('must not create shadow tree eagerly', () => {
            return createTestToCheckExistenceOfShadowTree((instance, hasShadowTree) => {
                expect(hasShadowTree()).to.be(false);
            });
        });
    });

    describe('element()', () => {
        it('must return an element', () => {
            const context = new BrowsingContext();
            return importComponentBase(context).then((ComponentBase) => {
                class SomeComponent extends ComponentBase { }
                let instance = new SomeComponent('some-component');
                expect(instance.element()).to.be.a(context.global.HTMLElement);
            });
        });

        it('must return an element whose component() matches the component', () => {
            const context = new BrowsingContext();
            return importComponentBase(context).then((ComponentBase) => {
                class SomeComponent extends ComponentBase { }
                let instance = new SomeComponent('some-component');
                expect(instance.element().component()).to.be(instance);
            });
        });

        it('must not create shadow tree eagerly', () => {
            return createTestToCheckExistenceOfShadowTree((instance, hasShadowTree) => {
                instance.element();
                expect(hasShadowTree()).to.be(false);
            });
        });
    });

    describe('content()', () => {
        it('must create shadow tree', () => {
            return createTestToCheckExistenceOfShadowTree((instance, hasShadowTree) => {
                instance.content();
                expect(hasShadowTree()).to.be(true);
            });
        });

        it('must return the same shadow tree each time it is called', () => {
            return createTestToCheckExistenceOfShadowTree((instance, hasShadowTree) => {
                expect(instance.content()).to.be(instance.content());
            });
        });

        it('must return the element matching the id if an id is specified', () => {
            return importComponentBase(new BrowsingContext()).then((ComponentBase) => {
                class SomeComponent extends ComponentBase {
                    static htmlTemplate() { return '<div id="part1" title="foo"></div><div id="part1"></div>'; }
                }
                ComponentBase.defineElement('some-component', SomeComponent);

                const instance = new SomeComponent;
                const part1 = instance.content('part1');
                expect(part1.localName).to.be('div');
                expect(part1.title).to.be('foo');
                expect(instance.content('part2')).to.be(null);
            });
        });

        it('it must create DOM tree from contentTemplate', async () => {
            const context = new BrowsingContext();
            const ComponentBase = await importComponentBase(context);
            class SomeComponent extends ComponentBase { };
            SomeComponent.contentTemplate = ['div', {id: 'container'}, 'hello, world'];
            const instance = new SomeComponent('some-component');
            const container = instance.content('container');
            expect(container).to.be.a(context.global.HTMLDivElement);
            expect(container.textContent).to.be('hello, world');
        });

        it('it must create content using derived class content template', async () => {
            const context = new BrowsingContext();
            const ComponentBase = await importComponentBase(context);

            class BaseClass extends ComponentBase { };
            BaseClass.contentTemplate = ['div', {id: 'container'}, 'base-class'];
            const baseInstance = new BaseClass('base-class');

            class DerivedClass extends BaseClass {};
            DerivedClass.contentTemplate = ['div', {id: 'container'}, 'derived-class'];
            const derivedInstance = new DerivedClass('derived-class');

            const baseContainer = baseInstance.content('container');
            expect(baseContainer).to.be.a(context.global.HTMLDivElement);
            expect(baseContainer.textContent).to.be('base-class');

            const derivedContainer = derivedInstance.content('container');
            expect(derivedContainer).to.be.a(context.global.HTMLDivElement);
            expect(derivedContainer.textContent).to.be('derived-class');
        });

        it('it must create stylsheet from styleTemplate', async () => {
            const context = new BrowsingContext();
            const ComponentBase = await importComponentBase(context);
            class SomeComponent extends ComponentBase { };
            SomeComponent.contentTemplate = ['span', 'hello, world'];
            SomeComponent.styleTemplate = {':host': {'font-weight': 'bold'}};
            const instance = new SomeComponent('some-component');
            context.document.body.append(instance.element());
            expect(context.global.getComputedStyle(instance.content().firstChild).fontWeight).to.be('bold');
        });
    });

    describe('part()', () => {
        it('must create shadow tree', () => {
            return createTestToCheckExistenceOfShadowTree((instance, hasShadowTree) => {
                instance.part('foo');
                expect(hasShadowTree()).to.be(true);
            });
        });

        it('must return the component matching the id if an id is specified', () => {
            return importComponentBase(new BrowsingContext()).then((ComponentBase) => {
                class SomeComponent extends ComponentBase { }
                ComponentBase.defineElement('some-component', SomeComponent);

                class OtherComponent extends ComponentBase {
                    static htmlTemplate() { return '<some-component id="foo"></some-component>'; }
                }
                ComponentBase.defineElement('other-component', OtherComponent);

                const otherComponent = new OtherComponent;
                const someComponent = otherComponent.part('foo');
                expect(someComponent).to.be.a(SomeComponent);
                expect(someComponent.element().id).to.be('foo');
                expect(otherComponent.part('foo')).to.be(someComponent);
                expect(otherComponent.part('bar')).to.be(null);
            });
        });
    });

    describe('dispatchAction()', () => {
        it('must invoke a callback specified in listenToAction', () => {
            return importComponentBase(new BrowsingContext()).then((ComponentBase) => {
                class SomeComponent extends ComponentBase { }
                ComponentBase.defineElement('some-component', SomeComponent);

                const instance = new SomeComponent;

                const calls = [];
                instance.listenToAction('action', (...args) => {
                    calls.push(args);
                });
                const object = {'foo': 1};
                instance.dispatchAction('action', 'bar', object, 5);

                expect(calls.length).to.be(1);
                expect(calls[0][0]).to.be('bar');
                expect(calls[0][1]).to.be(object);
                expect(calls[0][2]).to.be(5);
            });
        });

        it('must not do anything when there are no callbacks', () => {
            return importComponentBase(new BrowsingContext()).then((ComponentBase) => {
                class SomeComponent extends ComponentBase { }
                ComponentBase.defineElement('some-component', SomeComponent);

                const object = {'foo': 1};
                (new SomeComponent).dispatchAction('action', 'bar', object, 5);
            });
        });
    });

    describe('enqueueToRender()', () => {
        it('must not immediately call render()', () => {
            const context = new BrowsingContext();
            return importComponentBase(context).then((ComponentBase) => {
                context.global.requestAnimationFrame = () => {}

                let renderCallCount = 0;
                const SomeComponent = class extends ComponentBase {
                    render() { renderCallCount++; }
                }
                ComponentBase.defineElement('some-component', SomeComponent);

                (new SomeComponent).enqueueToRender();
                expect(renderCallCount).to.be(0);

                (new SomeComponent).enqueueToRender();
                expect(renderCallCount).to.be(0);
            });
        });

        it('must request an animation frame exactly once', () => {
            const context = new BrowsingContext();
            return importComponentBase(context).then((ComponentBase) => {
                let requestAnimationFrameCount = 0;
                context.global.requestAnimationFrame = () => { requestAnimationFrameCount++; }

                const SomeComponent = class extends ComponentBase { }
                ComponentBase.defineElement('some-component', SomeComponent);

                expect(requestAnimationFrameCount).to.be(0);
                let instance = new SomeComponent;
                instance.enqueueToRender();
                expect(requestAnimationFrameCount).to.be(1);

                instance.enqueueToRender();
                expect(requestAnimationFrameCount).to.be(1);

                (new SomeComponent).enqueueToRender();
                expect(requestAnimationFrameCount).to.be(1);

                const AnotherComponent = class extends ComponentBase { }
                ComponentBase.defineElement('another-component', AnotherComponent);
                (new AnotherComponent).enqueueToRender();
                expect(requestAnimationFrameCount).to.be(1);
            });
        });

        it('must invoke render() when the callback to requestAnimationFrame is called', () => {
            const context = new BrowsingContext();
            return importComponentBase(context).then((ComponentBase) => {
                let callback = null;
                context.global.requestAnimationFrame = (newCallback) => {
                    expect(callback).to.be(null);
                    expect(newCallback).to.not.be(null);
                    callback = newCallback;
                }

                let renderCalls = [];
                const SomeComponent = class extends ComponentBase {
                    render() {
                        renderCalls.push(this);
                    }
                }
                ComponentBase.defineElement('some-component', SomeComponent);

                expect(renderCalls.length).to.be(0);
                const instance = new SomeComponent;
                instance.enqueueToRender();
                instance.enqueueToRender();

                const anotherInstance = new SomeComponent;
                anotherInstance.enqueueToRender();
                expect(renderCalls.length).to.be(0);

                callback();

                expect(renderCalls.length).to.be(2);
                expect(renderCalls[0]).to.be(instance);
                expect(renderCalls[1]).to.be(anotherInstance);
            });
        });

        it('must immediately invoke render() on a component enqueued inside another render() call', () => {
            const context = new BrowsingContext();
            return importComponentBase(context).then((ComponentBase) => {
                let callback = null;
                context.global.requestAnimationFrame = (newCallback) => {
                    expect(callback).to.be(null);
                    expect(newCallback).to.not.be(null);
                    callback = newCallback;
                }

                let renderCalls = [];
                let instanceToEnqueue = null;
                const SomeComponent = class extends ComponentBase {
                    render() {
                        renderCalls.push(this);
                        if (instanceToEnqueue)
                            instanceToEnqueue.enqueueToRender();
                        instanceToEnqueue = null;
                    }
                }
                ComponentBase.defineElement('some-component', SomeComponent);

                expect(renderCalls.length).to.be(0);
                const instance = new SomeComponent;
                const anotherInstance = new SomeComponent;
                instance.enqueueToRender();
                instanceToEnqueue = anotherInstance;
                callback();
                callback = null;
                expect(renderCalls.length).to.be(2);
                expect(renderCalls[0]).to.be(instance);
                expect(renderCalls[1]).to.be(anotherInstance);
                renderCalls = [];

                instance.enqueueToRender();
                anotherInstance.enqueueToRender();
                instanceToEnqueue = instance;
                callback();
                expect(renderCalls.length).to.be(3);
                expect(renderCalls[0]).to.be(instance);
                expect(renderCalls[1]).to.be(anotherInstance);
                expect(renderCalls[2]).to.be(instance);
            });
        });

        it('must request a new animation frame once it exited the callback from requestAnimationFrame', () => {
            const context = new BrowsingContext();
            return importComponentBase(context).then((ComponentBase) => {
                let requestAnimationFrameCount = 0;
                let callback = null;
                context.global.requestAnimationFrame = (newCallback) => {
                    expect(callback).to.be(null);
                    expect(newCallback).to.not.be(null);
                    callback = newCallback;
                    requestAnimationFrameCount++;
                }

                let renderCalls = [];
                const SomeComponent = class extends ComponentBase {
                    render() { renderCalls.push(this); }
                }
                ComponentBase.defineElement('some-component', SomeComponent);

                const instance = new SomeComponent;
                const anotherInstance = new SomeComponent;
                expect(requestAnimationFrameCount).to.be(0);

                instance.enqueueToRender();
                expect(requestAnimationFrameCount).to.be(1);
                anotherInstance.enqueueToRender();
                expect(requestAnimationFrameCount).to.be(1);

                expect(renderCalls.length).to.be(0);
                callback();
                callback = null;
                expect(renderCalls.length).to.be(2);
                expect(renderCalls[0]).to.be(instance);
                expect(renderCalls[1]).to.be(anotherInstance);
                expect(requestAnimationFrameCount).to.be(1);

                anotherInstance.enqueueToRender();
                expect(requestAnimationFrameCount).to.be(2);
                instance.enqueueToRender();
                expect(requestAnimationFrameCount).to.be(2);

                expect(renderCalls.length).to.be(2);
                callback();
                callback = null;
                expect(renderCalls.length).to.be(4);
                expect(renderCalls[0]).to.be(instance);
                expect(renderCalls[1]).to.be(anotherInstance);
                expect(renderCalls[2]).to.be(anotherInstance);
                expect(renderCalls[3]).to.be(instance);
                expect(requestAnimationFrameCount).to.be(2);
            });
        });

    });

    describe('render()', () => {
        it('must create shadow tree', () => {
            return createTestToCheckExistenceOfShadowTree((instance, hasShadowTree) => {
                instance.render();
                expect(hasShadowTree()).to.be(true);
            });
        });

        it('must not create shadow tree when neither htmlTemplate nor cssTemplate are present', () => {
            return createTestToCheckExistenceOfShadowTree((instance, hasShadowTree) => {
                instance.render();
                expect(hasShadowTree()).to.be(false);
            }, {htmlTemplate: false, cssTemplate: false});
        });

        it('must create shadow tree when htmlTemplate is present and cssTemplate is not', () => {
            return createTestToCheckExistenceOfShadowTree((instance, hasShadowTree) => {
                instance.render();
                expect(hasShadowTree()).to.be(true);
            }, {htmlTemplate: true, cssTemplate: false});
        });

        it('must create shadow tree when cssTemplate is present and htmlTemplate is not', () => {
            return createTestToCheckExistenceOfShadowTree((instance, hasShadowTree) => {
                instance.render();
                expect(hasShadowTree()).to.be(true);
            }, {htmlTemplate: false, cssTemplate: true});
        });

        it('must invoke didConstructShadowTree after creating the shadow tree', () => {
            const context = new BrowsingContext();
            return importComponentBase(context).then((ComponentBase) => {
                let didConstructShadowTreeCount = 0;
                let htmlTemplateCount = 0;

                class SomeComponent extends ComponentBase {
                    didConstructShadowTree()
                    {
                        expect(this.content()).to.be.a(context.global.ShadowRoot);
                        didConstructShadowTreeCount++;
                    }

                    static htmlTemplate()
                    {
                        htmlTemplateCount++;
                        return '';
                    }
                }
                ComponentBase.defineElement('some-component', SomeComponent);

                const instance = new SomeComponent;
                expect(didConstructShadowTreeCount).to.be(0);
                expect(htmlTemplateCount).to.be(0);
                instance.render();
                expect(didConstructShadowTreeCount).to.be(1);
                expect(htmlTemplateCount).to.be(1);
            });
        });
    });

    describe('createElement()', () => {

        it('should create an element of the specified name', () => {
            const context = new BrowsingContext();
            return importComponentBase(context).then((ComponentBase) => {
                const div = ComponentBase.createElement('div');
                expect(div).to.be.a(context.global.HTMLDivElement);
            });
        });

        it('should allow to create an element with attributes to be null or undefined', () => {
            const context = new BrowsingContext();
            return importComponentBase(context).then((ComponentBase) => {
                const input = ComponentBase.createElement('input', {'title': null, 'required': undefined});
                expect(input).to.be.a(context.global.HTMLInputElement);
                expect(input.attributes.length).to.be(2);
                expect(input.attributes[0].localName).to.be('title');
                expect(input.attributes[0].value).to.be('null');
                expect(input.attributes[1].localName).to.be('required');
                expect(input.attributes[1].value).to.be('undefined');
            });
        });

        it('should create an element with the specified attributes', () => {
            const context = new BrowsingContext();
            return importComponentBase(context).then((ComponentBase) => {
                const input = ComponentBase.createElement('input', {'title': 'hi', 'id': 'foo', 'required': false, 'checked': true});
                expect(input).to.be.a(context.global.HTMLInputElement);
                expect(input.attributes.length).to.be(3);
                expect(input.attributes[0].localName).to.be('title');
                expect(input.attributes[0].value).to.be('hi');
                expect(input.attributes[1].localName).to.be('id');
                expect(input.attributes[1].value).to.be('foo');
                expect(input.attributes[2].localName).to.be('checked');
                expect(input.attributes[2].value).to.be('');
            });
        });

        it('should create an element with the specified event handlers and attributes', () => {
            const context = new BrowsingContext();
            return importComponentBase(context).then((ComponentBase) => {
                let clickCount = 0;
                const div = ComponentBase.createElement('div', {'title': 'hi', 'onclick': () => clickCount++});
                expect(div).to.be.a(context.global.HTMLDivElement);
                expect(div.attributes.length).to.be(1);
                expect(div.attributes[0].localName).to.be('title');
                expect(div.attributes[0].value).to.be('hi');
                expect(clickCount).to.be(0);
                div.click();
                expect(clickCount).to.be(1);
            });
        });

        it('should create an element with the specified children when there is no attribute specified', () => {
            const context = new BrowsingContext();
            return importComponentBase(context).then((ComponentBase) => {
                const element = ComponentBase.createElement;
                const span = element('span');
                const div = element('div', [span, 'hi']);
                expect(div).to.be.a(context.global.HTMLDivElement);
                expect(div.attributes.length).to.be(0);
                expect(div.childNodes.length).to.be(2);
                expect(div.childNodes[0]).to.be(span);
                expect(div.childNodes[1]).to.be.a(context.global.Text);
                expect(div.childNodes[1].data).to.be('hi');
            });
        });

        it('should create an element with the specified children when the second argument is a span', () => {
            const context = new BrowsingContext();
            return importComponentBase(context).then((ComponentBase) => {
                const element = ComponentBase.createElement;
                const span = element('span');
                const div = element('div', span);
                expect(div).to.be.a(context.global.HTMLDivElement);
                expect(div.attributes.length).to.be(0);
                expect(div.childNodes.length).to.be(1);
                expect(div.childNodes[0]).to.be(span);
            });
        });

        it('should create an element with the specified children when the second argument is a Text node', () => {
            const context = new BrowsingContext();
            return importComponentBase(context).then((ComponentBase) => {
                const element = ComponentBase.createElement;
                const text = context.document.createTextNode('hi');
                const div = element('div', text);
                expect(div).to.be.a(context.global.HTMLDivElement);
                expect(div.attributes.length).to.be(0);
                expect(div.childNodes.length).to.be(1);
                expect(div.childNodes[0]).to.be(text);
            });
        });

        it('should create an element with the specified children when the second argument is a component', () => {
            const context = new BrowsingContext();
            return importComponentBase(context).then((ComponentBase) => {
                class SomeComponent extends ComponentBase { };
                ComponentBase.defineElement('some-component', SomeComponent);
                const element = ComponentBase.createElement;
                const component = new SomeComponent;
                const div = element('div', component);
                expect(div).to.be.a(context.global.HTMLDivElement);
                expect(div.attributes.length).to.be(0);
                expect(div.childNodes.length).to.be(1);
                expect(div.childNodes[0]).to.be(component.element());
            });
        });

        it('should create an element with the specified attributes and children', () => {
            const context = new BrowsingContext();
            return importComponentBase(context).then((ComponentBase) => {
                const element = ComponentBase.createElement;
                const span = element('span');
                const div = element('div', {'lang': 'en'}, [span, 'hi']);
                expect(div).to.be.a(context.global.HTMLDivElement);
                expect(div.attributes.length).to.be(1);
                expect(div.attributes[0].localName).to.be('lang');
                expect(div.attributes[0].value).to.be('en');
                expect(div.childNodes.length).to.be(2);
                expect(div.childNodes[0]).to.be(span);
                expect(div.childNodes[1]).to.be.a(context.global.Text);
                expect(div.childNodes[1].data).to.be('hi');
            });
        });

    });

    describe('defineElement()', () => {

        it('must define a custom element with a class of an appropriate name', () => {
            const context = new BrowsingContext();
            return importComponentBase(context).then((ComponentBase) => {
                class SomeComponent extends ComponentBase { }
                ComponentBase.defineElement('some-component', SomeComponent);

                let elementClass = context.global.customElements.get('some-component');
                expect(elementClass).to.be.a('function');
                expect(elementClass.name).to.be('SomeComponentElement');
            });
        });

        it('must define a custom element that can be instantiated via document.createElement', () => {
            const context = new BrowsingContext();
            return importComponentBase(context).then((ComponentBase) => {
                let instances = [];
                class SomeComponent extends ComponentBase {
                    constructor() {
                        super();
                        instances.push(this);
                    }
                }
                ComponentBase.defineElement('some-component', SomeComponent);

                expect(instances.length).to.be(0);
                let element = context.document.createElement('some-component');
                expect(instances.length).to.be(1);

                expect(element).to.be.a(context.global.HTMLElement);
                expect(element.component()).to.be(instances[0]);
                expect(instances[0].element()).to.be(element);
                expect(instances.length).to.be(1);
            });
        });

        it('must define a custom element that can be instantiated via new', () => {
            const context = new BrowsingContext();
            return importComponentBase(context).then((ComponentBase) => {
                let instances = [];
                class SomeComponent extends ComponentBase {
                    constructor() {
                        super();
                        instances.push(this);
                    }
                }
                ComponentBase.defineElement('some-component', SomeComponent);

                expect(instances.length).to.be(0);
                let component = new SomeComponent;
                expect(instances.length).to.be(1);

                expect(component).to.be(instances[0]);
                expect(component.element()).to.be.a(context.global.HTMLElement);
                expect(component.element().component()).to.be(component);
                expect(instances.length).to.be(1);
            });
        });

    });

    describe('_ensureShadowTree', () => {
        it('should parse derived component after parsing base component', async () => {
            const context = new BrowsingContext();
            const ComponentBase = await importComponentBase(context);
            class DerivedComponent extends ComponentBase {};
            const baseInstance = new ComponentBase;
            expect(ComponentBase.hasOwnProperty('_parsed')).to.be(false);
            expect(DerivedComponent.hasOwnProperty('_parsed')).to.be(false);

            baseInstance._ensureShadowTree();
            expect(ComponentBase.hasOwnProperty('_parsed')).to.be(true);
            expect(DerivedComponent.hasOwnProperty('_parsed')).to.be(false);
            expect(!!ComponentBase._parsed).to.be(true);
            expect(!!DerivedComponent._parsed).to.be(true);

            const derivedInstance = new DerivedComponent;
            derivedInstance._ensureShadowTree();
            expect(ComponentBase.hasOwnProperty('_parsed')).to.be(true);
            expect(DerivedComponent.hasOwnProperty('_parsed')).to.be(true);
            expect(!!ComponentBase._parsed).to.be(true);
            expect(!!DerivedComponent._parsed).to.be(true);
        });
    });

});
