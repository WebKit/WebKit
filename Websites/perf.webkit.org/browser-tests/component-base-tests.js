
describe('ComponentBase', function() {

    function createTestToCheckExistenceOfShadowTree(callback, options = {htmlTemplate: false, cssTemplate: true})
    {
        const context = new BrowsingContext();
        return context.importScript('../public/v3/components/base.js', 'ComponentBase').then((ComponentBase) => {
            class SomeComponent extends ComponentBase { }
            if (options.htmlTemplate)
                SomeComponent.htmlTemplate = () => { return '<div style="height: 10px;"></div>'; };
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
            return new BrowsingContext().importScript('../public/v3/components/base.js', 'ComponentBase').then((ComponentBase) => {
                expect(ComponentBase).toBeA('function');
            });
        });

        it('can be instantiated', () => {
            return new BrowsingContext().importScript('../public/v3/components/base.js', 'ComponentBase').then((ComponentBase) => {
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
            return context.importScript('../public/v3/components/base.js', 'ComponentBase').then((ComponentBase) => {
                class SomeComponent extends ComponentBase { }
                let instance = new SomeComponent('some-component');
                expect(instance.element()).toBeA(context.global.HTMLElement);
            });
        });

        it('must return an element whose component() matches the component', () => {
            return new BrowsingContext().importScript('../public/v3/components/base.js', 'ComponentBase').then((ComponentBase) => {
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
    });

    describe('defineElement()', () => {

        it('must define a custom element with a class of an appropriate name', () => {
            const context = new BrowsingContext();
            return context.importScript('../public/v3/components/base.js', 'ComponentBase').then((ComponentBase) => {
                class SomeComponent extends ComponentBase { }
                ComponentBase.defineElement('some-component', SomeComponent);

                let elementClass = context.global.customElements.get('some-component');
                expect(elementClass).toBeA('function');
                expect(elementClass.name).toBe('SomeComponentElement');
            });
        });

        it('must define a custom element that can be instantiated via document.createElement', () => {
            const context = new BrowsingContext();
            return context.importScript('../public/v3/components/base.js', 'ComponentBase').then((ComponentBase) => {
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
            return context.importScript('../public/v3/components/base.js', 'ComponentBase').then((ComponentBase) => {
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

    });

});
