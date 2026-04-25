
class CommonComponentBase {

    renderReplace(element, content) { CommonComponentBase.renderReplace(element, content); }

    // FIXME: Deprecate these static functions.
    static renderReplace(element, content)
    {
        element.textContent = '';
        if (content)
            ComponentBase._addContentToElement(element, content);
    }

    _recursivelyUpgradeUnknownElements(parent, findUpgrade, didConstructComponent = () => { })
    {
        let nextSibling;
        for (let child of parent.childNodes) {
            const componentClass = findUpgrade(child);
            if (componentClass) {
                const intance = this._upgradeUnknownElement(parent, child, componentClass);
                didConstructComponent(intance);
            }
            if (child.childNodes)
                this._recursivelyUpgradeUnknownElements(child, findUpgrade, didConstructComponent);
        }
    }

    _upgradeUnknownElement(parent, unknownElement, componentClass)
    {
        const instance = new componentClass;
        const newElement = instance.element();

        for (let i = 0; i < unknownElement.attributes.length; i++) {
            const attr = unknownElement.attributes[i];
            newElement.setAttribute(attr.name, attr.value);
        }
        parent.replaceChild(newElement, unknownElement);

        for (const child of Array.from(unknownElement.childNodes))
            newElement.appendChild(child);

        return instance;
    }

    static _constructStylesheetFromTemplate(styleTemplate, didCreateRule = (selector, rule) => selector)
    {
        let stylesheet = '';
        for (const selector in styleTemplate) {
            const rules = styleTemplate[selector];

            let ruleText = '';
            for (const property in rules) {
                const value = rules[property];
                ruleText += `    ${property}: ${value};\n`;
            }

            const modifiedSelector = didCreateRule(selector, ruleText);

            stylesheet += modifiedSelector + ' {\n' + ruleText + '}\n\n';
        }
        return stylesheet;
    }

    static _constructNodeTreeFromTemplate(template, didCreateElement = (element) => { })
    {
        if (typeof(template) == 'string')
            return [CommonComponentBase._context.createTextNode(template)];
        console.assert(Array.isArray(template));
        if (typeof(template[0]) == 'string') {
            const tagName = template[0];
            let attributes = {};
            let content = null;
            if (Array.isArray(template[1])) {
                content = template[1];
            } else {
                attributes = template[1];
                content = template[2];
            }
            const element = this.createElement(tagName, attributes);
            didCreateElement(element);
            const children = content && content.length ? this._constructNodeTreeFromTemplate(content, didCreateElement) : [];
            for (const child of children)
                element.appendChild(child);
            return [element];
        } else {
            let result = [];
            for (const item of template) {
                if (typeof(item) == 'string')
                    result.push(CommonComponentBase._context.createTextNode(item));
                else
                    result = result.concat(this._constructNodeTreeFromTemplate(item, didCreateElement));
            }
            return result;
        }
    }

    createElement(name, attributes, content) { return CommonComponentBase.createElement(name, attributes, content); }

    static createElement(name, attributes, content)
    {
        const element = CommonComponentBase._context.createElement(name);
        if (!content && (Array.isArray(attributes) || CommonComponentBase._isNode(attributes)
            || attributes instanceof CommonComponentBase._baseClass || typeof(attributes) != 'object')) {
            content = attributes;
            attributes = {};
        }

        if (attributes) {
            for (const name in attributes) {
                if (name.startsWith('on'))
                    element.addEventListener(name.substring(2), attributes[name]);
                else if (attributes[name] === true)
                    element.setAttribute(name, '');
                else if (attributes[name] !== false)
                    element.setAttribute(name, attributes[name]);
            }
        }
        
        if (content)
            CommonComponentBase._addContentToElement(element, content);

        return element;
    }

    static _addContentToElement(element, content)
    {
        if (Array.isArray(content)) {
            for (var nestedChild of content)
                this._addContentToElement(element, nestedChild);
        } else if (CommonComponentBase._isNode(content))
            element.appendChild(content);
         else if (content instanceof CommonComponentBase._baseClass)
            element.appendChild(content.element());
        else
            element.appendChild(CommonComponentBase._context.createTextNode(content));
    }

    createLink(content, titleOrCallback, callback, isExternal, tabIndex = null)
    {
        return CommonComponentBase.createLink(content, titleOrCallback, callback, isExternal, tabIndex);
    }

    static createLink(content, titleOrCallback, callback, isExternal, tabIndex = null)
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

        if (tabIndex)
            attributes['tabindex'] = tabIndex;

        if (typeof(callback) === 'string')
            attributes['href'] = callback;
        else
            attributes['onclick'] = CommonComponentBase._baseClass.createEventHandler(callback);

        if (isExternal)
            attributes['target'] = '_blank';
        return CommonComponentBase.createElement('a', attributes, content);
    }
};

CommonComponentBase._context = null;
CommonComponentBase._isNode = null;
CommonComponentBase._baseClass = null;

if (typeof module != 'undefined')
    module.exports.CommonComponentBase = CommonComponentBase;
