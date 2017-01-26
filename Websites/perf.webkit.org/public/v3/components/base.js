
class ComponentBase {
    constructor(name)
    {
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

        if (!window.customElements && new.target.enqueueToRenderOnResize)
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

        do {
            const currentSet = [...ComponentBase._componentsToRender];
            ComponentBase._componentsToRender.clear();
            for (let component of currentSet) {
                Instrumentation.startMeasuringTime('ComponentBase', 'renderingTimerDidFire.render');
                component.render();
                Instrumentation.endMeasuringTime('ComponentBase', 'renderingTimerDidFire.render');
            }
        } while (ComponentBase._componentsToRender.size);
        ComponentBase._componentsToRender = null;

        Instrumentation.endMeasuringTime('ComponentBase', 'renderingTimerDidFire');
    }

    static _connectedComponentToRenderOnResize(component)
    {
        if (!ComponentBase._componentsToRenderOnResize) {
            ComponentBase._componentsToRenderOnResize = new Set;
            window.addEventListener('resize', () => {
                for (let component of ComponentBase._componentsToRenderOnResize)
                    component.enqueueToRender();
            });
        }
        ComponentBase._componentsToRenderOnResize.add(component);
    }

    static _disconnectedComponentToRenderOnResize(component)
    {
        ComponentBase._componentsToRenderOnResize.delete(component);
    }

    renderReplace(element, content) { ComponentBase.renderReplace(element, content); }

    static renderReplace(element, content)
    {
        element.innerHTML = '';
        if (content)
            ComponentBase._addContentToElement(element, content);
    }

    _ensureShadowTree()
    {
        if (this._shadow)
            return;

        const newTarget = this.__proto__.constructor;
        const htmlTemplate = newTarget['htmlTemplate'];
        const cssTemplate = newTarget['cssTemplate'];

        if (!htmlTemplate && !cssTemplate)
            return;

        const shadow = this._element.attachShadow({mode: 'closed'});

        if (htmlTemplate) {
            const template = document.createElement('template');
            template.innerHTML = newTarget.htmlTemplate();
            shadow.appendChild(document.importNode(template.content, true));
            this._recursivelyReplaceUnknownElementsByComponents(shadow);
        }

        if (cssTemplate) {
            const style = document.createElement('style');
            style.textContent = newTarget.cssTemplate();
            shadow.appendChild(style);
        }
        this._shadow = shadow;
        this.didConstructShadowTree();
    }

    didConstructShadowTree() { }

    _recursivelyReplaceUnknownElementsByComponents(parent)
    {
        let nextSibling;
        for (let child = parent.firstChild; child; child = child.nextSibling) {
            if (child instanceof HTMLElement && !child.component) {
                const elementInterface = ComponentBase._componentByName.get(child.localName);
                if (elementInterface) {
                    const component = new elementInterface();
                    const newChild = component.element();

                    for (let i = 0; i < child.attributes.length; i++) {
                        const attr = child.attributes[i];
                        newChild.setAttribute(attr.name, attr.value);
                    }

                    parent.replaceChild(newChild, child);
                    child = newChild;
                }
            }
            this._recursivelyReplaceUnknownElementsByComponents(child);
        }
    }

    static defineElement(name, elementInterface)
    {
        ComponentBase._componentByName.set(name, elementInterface);
        ComponentBase._componentByClass.set(elementInterface, name);

        const enqueueToRenderOnResize = elementInterface.enqueueToRenderOnResize;

        if (!window.customElements)
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

    static createElement(name, attributes, content)
    {
        var element = document.createElement(name);
        if (!content && (attributes instanceof Array || attributes instanceof Node
            || attributes instanceof ComponentBase || typeof(attributes) != 'object')) {
            content = attributes;
            attributes = {};
        }

        if (attributes) {
            for (var name in attributes) {
                if (name.startsWith('on'))
                    element.addEventListener(name.substring(2), attributes[name]);
                else
                    element.setAttribute(name, attributes[name]);
            }
        }

        if (content)
            ComponentBase._addContentToElement(element, content);

        return element;
    }

    static _addContentToElement(element, content)
    {
        if (content instanceof Array) {
            for (var nestedChild of content)
                this._addContentToElement(element, nestedChild);
        } else if (content instanceof Node)
            element.appendChild(content);
         else if (content instanceof ComponentBase)
            element.appendChild(content.element());
        else
            element.appendChild(document.createTextNode(content));
    }

    static createLink(content, titleOrCallback, callback, isExternal)
    {
        var title = titleOrCallback;
        if (callback === undefined) {
            title = content;
            callback = titleOrCallback;
        }

        var attributes = {
            href: '#',
            title: title,
        };

        if (typeof(callback) === 'string')
            attributes['href'] = callback;
        else
            attributes['onclick'] = ComponentBase.createEventHandler(callback);

        if (isExternal)
            attributes['target'] = '_blank';
        return ComponentBase.createElement('a', attributes, content);
    }

    createEventHandler(callback) { return ComponentBase.createEventHandler(callback); }
    static createEventHandler(callback)
    {
        return function (event) {
            event.preventDefault();
            event.stopPropagation();
            callback.call(this, event);
        };
    }
}

ComponentBase._componentByName = new Map;
ComponentBase._componentByClass = new Map;
ComponentBase._currentlyConstructedByInterface = new Map;
ComponentBase._componentsToRender = null;
ComponentBase._componentsToRenderOnResize = null;
