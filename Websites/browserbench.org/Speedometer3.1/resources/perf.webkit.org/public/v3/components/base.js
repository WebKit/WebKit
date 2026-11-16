
class ComponentBase extends CommonComponentBase {
    constructor(name)
    {
        super();
        this._componentName = name || ComponentBase._componentByClass.get(new.target);

        const currentlyConstructed = ComponentBase._currentlyConstructedByInterface;
        let element = currentlyConstructed.get(new.target);
        if (!element) {
            currentlyConstructed.set(new.target, this);
            element = document.createElement(this._componentName);
            currentlyConstructed.delete(new.target);
        }
        element.component = () => { return this };

        this._element = element;
        this._shadow = null;
        this._actionCallbacks = new Map;
        this._oldSizeToCheckForResize = null;

        if (!ComponentBase.useNativeCustomElements)
            element.addEventListener('DOMNodeInsertedIntoDocument', () => this.enqueueToRender());
        if (!ComponentBase.useNativeCustomElements && new.target.enqueueToRenderOnResize)
            ComponentBase._connectedComponentToRenderOnResize(this);
    }

    element() { return this._element; }
    content(id = null)
    {
        this._ensureShadowTree();
        if (this._shadow && id != null)
            return this._shadow.getElementById(id);
        return this._shadow;
    }

    part(id)
    {
        this._ensureShadowTree();
        if (!this._shadow)
            return null;
        const part = this._shadow.getElementById(id);
        if (!part)
            return null;
        return part.component();
    }

    dispatchAction(actionName, ...args)
    {
        const callback = this._actionCallbacks.get(actionName);
        if (callback)
            return callback.apply(this, args);
    }

    listenToAction(actionName, callback)
    {
        this._actionCallbacks.set(actionName, callback);
    }

    render() { this._ensureShadowTree(); }

    enqueueToRender()
    {
        Instrumentation.startMeasuringTime('ComponentBase', 'updateRendering');

        if (!ComponentBase._componentsToRender) {
            ComponentBase._componentsToRender = new Set;
            requestAnimationFrame(() => ComponentBase.renderingTimerDidFire());
        }
        ComponentBase._componentsToRender.add(this);

        Instrumentation.endMeasuringTime('ComponentBase', 'updateRendering');
    }

    static renderingTimerDidFire()
    {
        Instrumentation.startMeasuringTime('ComponentBase', 'renderingTimerDidFire');

        const componentsToRender = ComponentBase._componentsToRender;
        this._renderLoop();
        if (ComponentBase._componentsToRenderOnResize) {
            const resizedComponents = this._resizedComponents(ComponentBase._componentsToRenderOnResize);
            if (resizedComponents.length) {
                ComponentBase._componentsToRender = new Set(resizedComponents);
                this._renderLoop();
            }
        }

        Instrumentation.endMeasuringTime('ComponentBase', 'renderingTimerDidFire');
    }

    static _renderLoop()
    {
        const componentsToRender = ComponentBase._componentsToRender;
        do {
            const currentSet = [...componentsToRender];
            componentsToRender.clear();
            const resizeSet = ComponentBase._componentsToRenderOnResize;
            for (let component of currentSet) {
                Instrumentation.startMeasuringTime('ComponentBase', 'renderingTimerDidFire.render');
                component.render();
                if (resizeSet && resizeSet.has(component)) {
                    const element = component.element();
                    component._oldSizeToCheckForResize = {width: element.offsetWidth, height: element.offsetHeight};
                }
                Instrumentation.endMeasuringTime('ComponentBase', 'renderingTimerDidFire.render');
            }
        } while (componentsToRender.size);
        ComponentBase._componentsToRender = null;
    }

    static _resizedComponents(componentSet)
    {
        if (!componentSet)
            return [];

        const resizedList = [];
        for (let component of componentSet) {
            const element = component.element();
            const width = element.offsetWidth;
            const height = element.offsetHeight;
            const oldSize = component._oldSizeToCheckForResize;
            if (oldSize && oldSize.width == width && oldSize.height == height)
                continue;
            resizedList.push(component);
        }
        return resizedList;
    }

    static _connectedComponentToRenderOnResize(component)
    {
        if (!ComponentBase._componentsToRenderOnResize) {
            ComponentBase._componentsToRenderOnResize = new Set;
            window.addEventListener('resize', () => {
                const resized = this._resizedComponents(ComponentBase._componentsToRenderOnResize);
                for (const component of resized)
                    component.enqueueToRender();
            });
        }
        ComponentBase._componentsToRenderOnResize.add(component);
    }

    static _disconnectedComponentToRenderOnResize(component)
    {
        ComponentBase._componentsToRenderOnResize.delete(component);
    }

    _ensureShadowTree()
    {
        if (this._shadow)
            return;

        const thisClass = this.__proto__.constructor;

        let content;
        let stylesheet;
        if (!thisClass.hasOwnProperty('_parsed') || !thisClass._parsed) {
            thisClass._parsed = true;

            const contentTemplate = thisClass['contentTemplate'];
            if (contentTemplate)
                content = ComponentBase._constructNodeTreeFromTemplate(contentTemplate);
            else if (thisClass.htmlTemplate) {
                const templateElement = document.createElement('template');
                templateElement.innerHTML = thisClass.htmlTemplate();
                content = [templateElement.content];
            }

            const styleTemplate = thisClass['styleTemplate'];
            if (styleTemplate)
                stylesheet = ComponentBase._constructStylesheetFromTemplate(styleTemplate);
            else if (thisClass.cssTemplate)
                stylesheet = thisClass.cssTemplate();

            thisClass._parsedContent = content;
            thisClass._parsedStylesheet = stylesheet;
        } else {
            content = thisClass._parsedContent;
            stylesheet = thisClass._parsedStylesheet;
        }

        if (!content && !stylesheet)
            return;

        const shadow = this._element.attachShadow({mode: 'closed'});

        if (content) {
            for (const node of content)
                shadow.appendChild(document.importNode(node, true));
            this._recursivelyUpgradeUnknownElements(shadow, (node) => {
                return node instanceof Element ? ComponentBase._componentByName.get(node.localName) : null;
            });
        }

        if (stylesheet) {
            const style = document.createElement('style');
            style.textContent = stylesheet;
            shadow.appendChild(style);
        }
        this._shadow = shadow;
        this.didConstructShadowTree();
    }

    didConstructShadowTree() { }

    static defineElement(name, elementInterface)
    {
        ComponentBase._componentByName.set(name, elementInterface);
        ComponentBase._componentByClass.set(elementInterface, name);

        const enqueueToRenderOnResize = elementInterface.enqueueToRenderOnResize;

        if (!ComponentBase.useNativeCustomElements)
            return;

        class elementClass extends HTMLElement {
            constructor()
            {
                super();

                const currentlyConstructed = ComponentBase._currentlyConstructedByInterface;
                const component = currentlyConstructed.get(elementInterface);
                if (component)
                    return; // ComponentBase's constructor will take care of the rest.

                currentlyConstructed.set(elementInterface, this);
                new elementInterface();
                currentlyConstructed.delete(elementInterface);
            }

            connectedCallback()
            {
                this.component().enqueueToRender();
                if (enqueueToRenderOnResize)
                    ComponentBase._connectedComponentToRenderOnResize(this.component());
            }

            disconnectedCallback()
            {
                if (enqueueToRenderOnResize)
                    ComponentBase._disconnectedComponentToRenderOnResize(this.component());
            }
        }

        const nameDescriptor = Object.getOwnPropertyDescriptor(elementClass, 'name');
        nameDescriptor.value = `${elementInterface.name}Element`;
        Object.defineProperty(elementClass, 'name', nameDescriptor);

        customElements.define(name, elementClass);
    }

    createEventHandler(callback, options={}) { return ComponentBase.createEventHandler(callback, options); }
    static createEventHandler(callback, options={})
    {
        return function (event) {
            if (!('preventDefault' in options) || options['preventDefault'])
                event.preventDefault();
            if (!('stopPropagation' in options) || options['stopPropagation'])
                event.stopPropagation();
            callback.call(this, event);
        };
    }
}

CommonComponentBase._context = document;
CommonComponentBase._isNode = (node) => node instanceof Node;
CommonComponentBase._baseClass = ComponentBase;

ComponentBase.useNativeCustomElements = !!window.customElements;
ComponentBase._componentByName = new Map;
ComponentBase._componentByClass = new Map;
ComponentBase._currentlyConstructedByInterface = new Map;
ComponentBase._componentsToRender = null;
ComponentBase._componentsToRenderOnResize = null;
