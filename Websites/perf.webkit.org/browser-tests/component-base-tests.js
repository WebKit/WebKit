
describe('ComponentBase', function() {

    function createTestToCheckExistenceOfShadowTree(callback, options = {htmlTemplate: false, cssTemplate: true})
    {
        const context = new BrowsingContext();
        return context.importScript('components/base.js', 'ComponentBase').then((ComponentBase) => {
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

    describe('constructor', () => {
        it('is a function', () => {
            return new BrowsingContext().importScript('components/base.js', 'ComponentBase').then((ComponentBase) => {
                expect(ComponentBase).toBeA('function');
            });
        });

        it('can be instantiated', () => {
            return new BrowsingContext().importScript('components/base.js', 'ComponentBase').then((ComponentBase) => {
                let callCount = 0;
                class SomeComponent extends ComponentBase {
                    constructor() {
                        super('some-component');
                        callCount++;
                    }
                }
                let instance = new SomeComponent;
                expect(instance).toBeA(ComponentBase);
                expect(instance).toBeA(SomeComponent);
                expect(callCount).toBe(1);
            });
        });

        it('must not create shadow tree eagerly', () => {
            return createTestToCheckExistenceOfShadowTree((instance, hasShadowTree) => {
                expect(hasShadowTree()).toBe(false);
            });
        });
    });

    describe('element()', () => {
        it('must return an element', () => {
            const context = new BrowsingContext();
            return context.importScript('components/base.js', 'ComponentBase').then((ComponentBase) => {
                class SomeComponent extends ComponentBase { }
                let instance = new SomeComponent('some-component');
                expect(instance.element()).toBeA(context.global.HTMLElement);
            });
        });

        it('must return an element whose component() matches the component', () => {
            return new BrowsingContext().importScript('components/base.js', 'ComponentBase').then((ComponentBase) => {
                class SomeComponent extends ComponentBase { }
                let instance = new SomeComponent('some-component');
                expect(instance.element().component()).toBe(instance);
            });
        });

        it('must not create shadow tree eagerly', () => {
            return createTestToCheckExistenceOfShadowTree((instance, hasShadowTree) => {
                instance.element();
                expect(hasShadowTree()).toBe(false);
            });
        });
    });

    describe('content()', () => {
        it('must create shadow tree', () => {
            return createTestToCheckExistenceOfShadowTree((instance, hasShadowTree) => {
                instance.content();
                expect(hasShadowTree()).toBe(true);
            });
        });

        it('must return the same shadow tree each time it is called', () => {
            return createTestToCheckExistenceOfShadowTree((instance, hasShadowTree) => {
                expect(instance.content()).toBe(instance.content());
            });
        });

        it('must return the element matching the id if an id is specified', () => {
            return new BrowsingContext().importScripts(['instrumentation.js', 'components/base.js'], 'ComponentBase').then((ComponentBase) => {
                class SomeComponent extends ComponentBase {
                    static htmlTemplate() { return '<div id="part1" title="foo"></div><div id="part1"></div>'; }
                }
                ComponentBase.defineElement('some-component', SomeComponent);

                const instance = new SomeComponent;
                const part1 = instance.content('part1');
                expect(part1.localName).toBe('div');
                expect(part1.title).toBe('foo');
                expect(instance.content('part2')).toBe(null);
            });
        });
    });

    describe('part()', () => {
        it('must create shadow tree', () => {
            return createTestToCheckExistenceOfShadowTree((instance, hasShadowTree) => {
                instance.part('foo');
                expect(hasShadowTree()).toBe(true);
            });
        });

        it('must return the component matching the id if an id is specified', () => {
            return new BrowsingContext().importScripts(['instrumentation.js', 'components/base.js'], 'ComponentBase').then((ComponentBase) => {
                class SomeComponent extends ComponentBase { }
                ComponentBase.defineElement('some-component', SomeComponent);

                class OtherComponent extends ComponentBase {
                    static htmlTemplate() { return '<some-component id="foo"></some-component>'; }
                }
                ComponentBase.defineElement('other-component', OtherComponent);

                const otherComponent = new OtherComponent;
                const someComponent = otherComponent.part('foo');
                expect(someComponent).toBeA(SomeComponent);
                expect(someComponent.element().id).toBe('foo');
                expect(otherComponent.part('foo')).toBe(someComponent);
                expect(otherComponent.part('bar')).toBe(null);
            });
        });
    });

    describe('dispatchAction()', () => {
        it('must invoke a callback specified in listenToAction', () => {
            return new BrowsingContext().importScripts(['instrumentation.js', 'components/base.js'], 'ComponentBase').then((ComponentBase) => {
                class SomeComponent extends ComponentBase { }
                ComponentBase.defineElement('some-component', SomeComponent);

                const instance = new SomeComponent;

                const calls = [];
                instance.listenToAction('action', (...args) => {
                    calls.push(args);
                });
                const object = {'foo': 1};
                instance.dispatchAction('action', 'bar', object, 5);

                expect(calls.length).toBe(1);
                expect(calls[0][0]).toBe('bar');
                expect(calls[0][1]).toBe(object);
                expect(calls[0][2]).toBe(5);
            });
        });

        it('must not do anything when there are no callbacks', () => {
            return new BrowsingContext().importScripts(['instrumentation.js', 'components/base.js'], 'ComponentBase').then((ComponentBase) => {
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
            return context.importScripts(['instrumentation.js', 'components/base.js'], 'ComponentBase').then((ComponentBase) => {
                context.global.requestAnimationFrame = () => {}

                let renderCallCount = 0;
                const SomeComponent = class extends ComponentBase {
                    render() { renderCallCount++; }
                }
                ComponentBase.defineElement('some-component', SomeComponent);

                (new SomeComponent).enqueueToRender();
                expect(renderCallCount).toBe(0);

                (new SomeComponent).enqueueToRender();
                expect(renderCallCount).toBe(0);
            });
        });

        it('must request an animation frame exactly once', () => {
            const context = new BrowsingContext();
            return context.importScripts(['instrumentation.js', 'components/base.js'], 'ComponentBase').then((ComponentBase) => {
                let requestAnimationFrameCount = 0;
                context.global.requestAnimationFrame = () => { requestAnimationFrameCount++; }

                const SomeComponent = class extends ComponentBase { }
                ComponentBase.defineElement('some-component', SomeComponent);

                expect(requestAnimationFrameCount).toBe(0);
                let instance = new SomeComponent;
                instance.enqueueToRender();
                expect(requestAnimationFrameCount).toBe(1);

                instance.enqueueToRender();
                expect(requestAnimationFrameCount).toBe(1);

                (new SomeComponent).enqueueToRender();
                expect(requestAnimationFrameCount).toBe(1);

                const AnotherComponent = class extends ComponentBase { }
                ComponentBase.defineElement('another-component', AnotherComponent);
                (new AnotherComponent).enqueueToRender();
                expect(requestAnimationFrameCount).toBe(1);
            });
        });

        it('must invoke render() when the callback to requestAnimationFrame is called', () => {
            const context = new BrowsingContext();
            return context.importScripts(['instrumentation.js', 'components/base.js'], 'ComponentBase').then((ComponentBase) => {
                let callback = null;
                context.global.requestAnimationFrame = (newCallback) => {
                    expect(callback).toBe(null);
                    expect(newCallback).toNotBe(null);
                    callback = newCallback;
                }

                let renderCalls = [];
                const SomeComponent = class extends ComponentBase {
                    render() {
                        renderCalls.push(this);
                    }
                }
                ComponentBase.defineElement('some-component', SomeComponent);

                expect(renderCalls.length).toBe(0);
                const instance = new SomeComponent;
                instance.enqueueToRender();
                instance.enqueueToRender();

                const anotherInstance = new SomeComponent;
                anotherInstance.enqueueToRender();
                expect(renderCalls.length).toBe(0);

                callback();

                expect(renderCalls.length).toBe(2);
                expect(renderCalls[0]).toBe(instance);
                expect(renderCalls[1]).toBe(anotherInstance);
            });
        });

        it('must immediately invoke render() on a component enqueued inside another render() call', () => {
            const context = new BrowsingContext();
            return context.importScripts(['instrumentation.js', 'components/base.js'], 'ComponentBase').then((ComponentBase) => {
                let callback = null;
                context.global.requestAnimationFrame = (newCallback) => {
                    expect(callback).toBe(null);
                    expect(newCallback).toNotBe(null);
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

                expect(renderCalls.length).toBe(0);
                const instance = new SomeComponent;
                const anotherInstance = new SomeComponent;
                instance.enqueueToRender();
                instanceToEnqueue = anotherInstance;
                callback();
                callback = null;
                expect(renderCalls.length).toBe(2);
                expect(renderCalls[0]).toBe(instance);
                expect(renderCalls[1]).toBe(anotherInstance);
                renderCalls = [];

                instance.enqueueToRender();
                anotherInstance.enqueueToRender();
                instanceToEnqueue = instance;
                callback();
                expect(renderCalls.length).toBe(3);
                expect(renderCalls[0]).toBe(instance);
                expect(renderCalls[1]).toBe(anotherInstance);
                expect(renderCalls[2]).toBe(instance);
            });
        });

        it('must request a new animation frame once it exited the callback from requestAnimationFrame', () => {
            const context = new BrowsingContext();
            return context.importScripts(['instrumentation.js', 'components/base.js'], 'ComponentBase').then((ComponentBase) => {
                let requestAnimationFrameCount = 0;
                let callback = null;
                context.global.requestAnimationFrame = (newCallback) => {
                    expect(callback).toBe(null);
                    expect(newCallback).toNotBe(null);
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
                expect(requestAnimationFrameCount).toBe(0);

                instance.enqueueToRender();
                expect(requestAnimationFrameCount).toBe(1);
                anotherInstance.enqueueToRender();
                expect(requestAnimationFrameCount).toBe(1);

                expect(renderCalls.length).toBe(0);
                callback();
                callback = null;
                expect(renderCalls.length).toBe(2);
                expect(renderCalls[0]).toBe(instance);
                expect(renderCalls[1]).toBe(anotherInstance);
                expect(requestAnimationFrameCount).toBe(1);

                anotherInstance.enqueueToRender();
                expect(requestAnimationFrameCount).toBe(2);
                instance.enqueueToRender();
                expect(requestAnimationFrameCount).toBe(2);

                expect(renderCalls.length).toBe(2);
                callback();
                callback = null;
                expect(renderCalls.length).toBe(4);
                expect(renderCalls[0]).toBe(instance);
                expect(renderCalls[1]).toBe(anotherInstance);
                expect(renderCalls[2]).toBe(anotherInstance);
                expect(renderCalls[3]).toBe(instance);
                expect(requestAnimationFrameCount).toBe(2);
            });
        });

    });

    describe('render()', () => {
        it('must create shadow tree', () => {
            return createTestToCheckExistenceOfShadowTree((instance, hasShadowTree) => {
                instance.render();
                expect(hasShadowTree()).toBe(true);
            });
        });

        it('must not create shadow tree when neither htmlTemplate nor cssTemplate are present', () => {
            return createTestToCheckExistenceOfShadowTree((instance, hasShadowTree) => {
                instance.render();
                expect(hasShadowTree()).toBe(false);
            }, {htmlTemplate: false, cssTemplate: false});
        });

        it('must create shadow tree when htmlTemplate is present and cssTemplate is not', () => {
            return createTestToCheckExistenceOfShadowTree((instance, hasShadowTree) => {
                instance.render();
                expect(hasShadowTree()).toBe(true);
            }, {htmlTemplate: true, cssTemplate: false});
        });

        it('must create shadow tree when cssTemplate is present and htmlTemplate is not', () => {
            return createTestToCheckExistenceOfShadowTree((instance, hasShadowTree) => {
                instance.render();
                expect(hasShadowTree()).toBe(true);
            }, {htmlTemplate: false, cssTemplate: true});
        });

        it('must invoke didConstructShadowTree after creating the shadow tree', () => {
            const context = new BrowsingContext();
            return context.importScripts(['instrumentation.js', 'components/base.js'], 'ComponentBase').then((ComponentBase) => {
                let didConstructShadowTreeCount = 0;
                let htmlTemplateCount = 0;

                class SomeComponent extends ComponentBase {
                    didConstructShadowTree()
                    {
                        expect(this.content()).toBeA(context.global.ShadowRoot);
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
                expect(didConstructShadowTreeCount).toBe(0);
                expect(htmlTemplateCount).toBe(0);
                instance.render();
                expect(didConstructShadowTreeCount).toBe(1);
                expect(htmlTemplateCount).toBe(1);
            });
        });
    });

    describe('defineElement()', () => {

        it('must define a custom element with a class of an appropriate name', () => {
            const context = new BrowsingContext();
            return context.importScript('components/base.js', 'ComponentBase').then((ComponentBase) => {
                class SomeComponent extends ComponentBase { }
                ComponentBase.defineElement('some-component', SomeComponent);

                let elementClass = context.global.customElements.get('some-component');
                expect(elementClass).toBeA('function');
                expect(elementClass.name).toBe('SomeComponentElement');
            });
        });

        it('must define a custom element that can be instantiated via document.createElement', () => {
            const context = new BrowsingContext();
            return context.importScript('components/base.js', 'ComponentBase').then((ComponentBase) => {
                let instances = [];
                class SomeComponent extends ComponentBase {
                    constructor() {
                        super();
                        instances.push(this);
                    }
                }
                ComponentBase.defineElement('some-component', SomeComponent);

                expect(instances.length).toBe(0);
                let element = context.document.createElement('some-component');
                expect(instances.length).toBe(1);

                expect(element).toBeA(context.global.HTMLElement);
                expect(element.component()).toBe(instances[0]);
                expect(instances[0].element()).toBe(element);
                expect(instances.length).toBe(1);
            });
        });

        it('must define a custom element that can be instantiated via new', () => {
            const context = new BrowsingContext();
            return context.importScript('components/base.js', 'ComponentBase').then((ComponentBase) => {
                let instances = [];
                class SomeComponent extends ComponentBase {
                    constructor() {
                        super();
                        instances.push(this);
                    }
                }
                ComponentBase.defineElement('some-component', SomeComponent);

                expect(instances.length).toBe(0);
                let component = new SomeComponent;
                expect(instances.length).toBe(1);

                expect(component).toBe(instances[0]);
                expect(component.element()).toBeA(context.global.HTMLElement);
                expect(component.element().component()).toBe(component);
                expect(instances.length).toBe(1);
            });
        });

        it('must enqueue a connected component to render upon a resize event if enqueueToRenderOnResize is true', () => {
            const context = new BrowsingContext();
            return context.importScripts(['instrumentation.js', 'components/base.js'], 'ComponentBase').then((ComponentBase) => {
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

                expect(requestAnimationFrameCount).toBe(0);
                const instance = new SomeComponent;
                context.global.dispatchEvent(new Event('resize'));
                context.document.body.appendChild(instance.element());
                context.global.dispatchEvent(new Event('resize'));
                expect(requestAnimationFrameCount).toBe(1);
            });
        });

        it('must not enqueue a disconnected component to render upon a resize event if enqueueToRenderOnResize is true', () => {
            const context = new BrowsingContext();
            return context.importScripts(['instrumentation.js', 'components/base.js'], 'ComponentBase').then((ComponentBase) => {
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
                expect(requestAnimationFrameCount).toBe(0);
                context.global.dispatchEvent(new Event('resize'));
                expect(requestAnimationFrameCount).toBe(0);
            });
        });

    });

});
