
// FIXME: ComponentBase should inherit from HTMLElement when custom elements API is available.
class ComponentBase {
    constructor(name)
    {
        this._element = document.createElement(name);
        this._element.component = (function () { return this; }).bind(this);
        this._shadow = this._constructShadowTree();
    }

    element() { return this._element; }
    content() { return this._shadow; }
    render() { }

    renderReplace(element, content)
    {
        element.innerHTML = '';
        if (content)
            ComponentBase._addContentToElement(element, content);
    }

    _constructShadowTree()
    {
        var newTarget = this.__proto__.constructor;

        var htmlTemplate = newTarget['htmlTemplate'];
        var cssTemplate = newTarget['cssTemplate'];

        if (!htmlTemplate && !cssTemplate)
            return null;

        var shadow = null;
        if ('attachShadow' in Element.prototype)
            shadow = this._element.attachShadow({mode: 'closed'});
        else if ('createShadowRoot' in Element.prototype) // Legacy Chromium API.
            shadow = this._element.createShadowRoot();
        else
            shadow = this._element;

        if (htmlTemplate) {
            var template = document.createElement('template');
            template.innerHTML = newTarget.htmlTemplate();
            shadow.appendChild(template.content.cloneNode(true));
            this._recursivelyReplaceUnknownElementsByComponents(shadow);
        }

        if (cssTemplate) {
            var style = document.createElement('style');
            style.textContent = newTarget.cssTemplate();
            shadow.appendChild(style);
        }

        return shadow;
    }

    _recursivelyReplaceUnknownElementsByComponents(parent)
    {
        if (!ComponentBase._map)
            return;

        var nextSibling;
        for (var child = parent.firstChild; child; child = child.nextSibling) {
            if (child instanceof HTMLUnknownElement || child instanceof HTMLElement) {
                var elementInterface = ComponentBase._map[child.localName];
                if (elementInterface) {
                    var component = new elementInterface();
                    var newChild = component.element();
                    parent.replaceChild(newChild, child);
                    child = newChild;
                }
            }
            this._recursivelyReplaceUnknownElementsByComponents(child);
        }
    }

    static isElementInViewport(element)
    {
        var viewportHeight = window.innerHeight;
        var boundingRect = element.getBoundingClientRect();
        if (viewportHeight < boundingRect.top || boundingRect.bottom < 0
            || !boundingRect.width || !boundingRect.height)
            return false;
        return true;
    }

    static defineElement(name, elementInterface)
    {
        if (!ComponentBase._map)
            ComponentBase._map = {};
        ComponentBase._map[name] = elementInterface;
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
            attributes['onclick'] = ComponentBase.createActionHandler(callback);

        if (isExternal)
            attributes['target'] = '_blank';
        return ComponentBase.createElement('a', attributes, content);
    }

    static createActionHandler(callback)
    {
        return function (event) {
            event.preventDefault();
            event.stopPropagation();
            callback.call(this, event);
        };
    }
}

ComponentBase.css = Symbol();
ComponentBase.html = Symbol();
ComponentBase.map = {};
