
const MarkupDocument = new class MarkupDocument {
    constructor()
    {
        this._nodeId = 1;
    }

    createContentRoot(host)
    {
        const id = this._nodeId++;
        return new MarkupContentRoot(id, host);
    }

    createElement(name)
    {
        const id = this._nodeId++;
        return new MarkupElement(id, name);
    }

    createTextNode(data)
    {
        const id = this._nodeId++;
        const text = new MarkupText(id);
        text.data = data;
        return text;
    }

    _idForClone(original)
    {
        console.assert(original instanceof MarkupNode);
        return this._nodeId++;
    }

    reset()
    {
        this._nodeId = 1;
    }

    markup(node)
    {
        console.assert(node instanceof MarkupNode);
        return node._markup();
    }

    escapeAttributeValue(string)
    {
        return this.escapeNodeData(string).replace(/\"/g, '&quod8;').replace(/\'/g, '&apos;');
    }

    escapeNodeData(string)
    {
        return string.replace(/&/g, '&amp;').replace(/\</g, '&lt;').replace(/\>/g, '&gt;');
    }
}

class MarkupNode {
    constructor(id)
    {
        console.assert(typeof(id) == 'number');
        this._id = id;
        this._parentNode = null;
    }

    _markup()
    {
        throw 'NotImplemented';
    }

    clone()
    {
        throw 'NotImplemented';
    }

    _cloneNodeData(clonedNode)
    {
        console.assert(typeof(clonedNode._id) == 'number');
        console.assert(this._id != clonedNode._id);
        console.assert(clonedNode._parentNode == null);
    }

    remove()
    {
        const parentNode = this._parentNode;
        if (parentNode)
            parentNode.removeChild(this);
    }
}

class MarkupParentNode extends MarkupNode {
    constructor(id)
    {
        super(id);
        this._childNodes = [];
    }

    get childNodes() { return this._childNodes.slice(0); }

    _cloneNodeData(clonedNode)
    {
        super._cloneNodeData(clonedNode);
        clonedNode._childNodes = this._childNodes.map((child) => {
            const clonedChild = child.clone();
            clonedChild._parentNode = clonedNode;
            return clonedChild;
        });
    }

    appendChild(child)
    {
        if (child._parentNode == this)
            return;

        if (child._parentNode)
            child.remove();

        console.assert(child._parentNode == null);
        this._childNodes.push(child);
        child._parentNode = this;
    }

    removeChild(child)
    {
        if (child._parentNode != this)
            return;
        const index = this._childNodes.indexOf(child);
        console.assert(index >= 0);
        this._childNodes.splice(index, 1);
        child._parentNode = null;
    }

    removeAllChildren()
    {
        for (const child of this._childNodes)
            child._parentNode = null;
        this._childNodes = [];
    }

    replaceChild(newChild, oldChild)
    {
        if (oldChild._parentNode != this)
            throw 'Invalid operation';

        if (newChild._parentNode)
            newChild.remove();
        console.assert(newChild._parentNode == null);

        const index = this._childNodes.indexOf(oldChild);
        console.assert(index >= 0);
        this._childNodes.splice(index, 1, newChild);
        oldChild._parentNode = null;
        newChild._parentNode = this;
    }
}

class MarkupContentRoot extends MarkupParentNode {
    constructor(id, host)
    {
        console.assert(host instanceof MarkupElement);
        console.assert(host._contentRoot == null);
        super(id);
        this._hostElement = null;
        host._contentRoot = this;
    }

    _markup()
    {
        let result = '';
        for (const child of this._childNodes)
            result += child._markup();
        return result;
    }
}

class MarkupElement extends MarkupParentNode {
    constructor(id, name)
    {
        super(id);
        console.assert(typeof(name) == 'string');
        this._name = name;
        this._attributes = new Map;
        this._value = null;
        this._styleProxy = null;
        this._inlineStyleProperties = new Map;
        this._contentRoot = null;
    }

    get id() { return this.getAttribute('id'); }
    get localName() { return this._name; }

    clone()
    {
        const clonedNode = new MarkupElement(MarkupDocument._idForClone(this), this._name);
        super._cloneNodeData(clonedNode);
        for (const [name, value] of this._attributes)
            clonedNode._attributes.set(name, value);
        clonedNode._value = this._value;
        for (const [name, value] of this._inlineStyleProperties)
            clonedNode._inlineStyleProperties.set(name, value);
        if (this._contentRoot) {
            const clonedContentRoot = new MarkupContentRoot(MarkupDocument._idForClone(this._contentRoot), clonedNode);
            this._contentRoot._cloneNodeData(clonedContentRoot);
        }
        return clonedNode;
    }

    appendChild(child)
    {
        if (MarkupElement.selfClosingNames.includes(this._name))
            throw 'The operation is not supported';
        super.appendChild(child);
    }

    addEventListener(name, callback)
    {
        throw 'The operation is not supported';
    }

    setAttribute(name, value = null)
    {
        if (name == 'style')
            this._inlineStyleProperties.clear();
        this._attributes.set(name.toString(), '' + value);
    }
    
    getAttribute(name, value)
    {
        if (name == 'style' && this._inlineStyleProperties.size)
            return this._serializeStyle();
        return this._attributes.get(name);
    }

    get attributes()
    {
        // FIXME: Add the support for named property.
        const result = [];
        for (const [name, value] of this._attributes)
            result.push({localName: name, name, value});
        return result;
    }

    get textContent()
    {
        let result = '';
        for (const node of this._childNodes) {
            if (node instanceof MarkupText)
                result += node.data;
        }
        return result;
    }

    set textContent(newContent)
    {
        this.removeAllChildren();
        if (newContent)
            this.appendChild(MarkupDocument.createTextNode(newContent));
    }

    _serializeStyle()
    {
        let styleValue = '';
        for (const [name, value] of this._inlineStyleProperties)
            styleValue += (styleValue ? '; ' : '') + name + ': ' + value;
        return styleValue;
    }

    _markup()
    {
        let markup = '<' + this._name;
        if (this._styleProxy && this._inlineStyleProperties.size)
            markup += ` style="${MarkupDocument.escapeAttributeValue(this._serializeStyle())}"`;
        for (const [name, value] of this._attributes)
            markup += ` ${name}="${MarkupDocument.escapeAttributeValue(value)}"`;
        markup += '>';
        if (this._contentRoot)
            markup += this._contentRoot._markup();
        else {
            for (const child of this._childNodes)
                markup += child._markup();
        }
        if (!MarkupElement.selfClosingNames.includes(this._name))
            markup += '</' + this._name + '>';
        return markup;
    }

    get value()
    {
        if (this._name != 'input')
            throw 'The operation is not supported';
        return this._value;
    }
    set value(value)
    {
        if (this._name != 'input')
            throw 'The operation is not supported';
        this._value = value.toString();
    }

    get style()
    {
        if (!this._styleProxy) {
            const proxyTarget = {};
            const cssPropertyFromJSProperty = (jsPropertyName) => {
                let cssPropertyName = '';
                for (let i = 0; i < jsPropertyName.length; i++) {
                    const currentChar = jsPropertyName.charAt(i);
                    if ('A' <= currentChar && currentChar <= 'Z')
                        cssPropertyName += '-' + currentChar.toLowerCase();
                    else
                        cssPropertyName += currentChar;
                }
                return cssPropertyName;
            };
            this._styleProxy = new Proxy(proxyTarget, {
                get: (target, property) => {
                    throw 'The operation is not supported';
                },
                set: (target, property, value) => {
                    this._inlineStyleProperties.set(cssPropertyFromJSProperty(property), value);
                    return true;
                },
            });
        }
        return this._styleProxy;
    }

    set style(value)
    {
        throw 'This operation is not supported';
    }

    static get selfClosingNames() { return ['img', 'br', 'meta', 'link']; }
}

class MarkupText extends MarkupNode {
    constructor(id)
    {
        super(id);
        this._data = null;
    }

    clone()
    {
        const clonedNode = new MarkupText(MarkupDocument._idForClone(this));
        clonedNode._data = this._data;
        return clonedNode;
    }

    _markup()
    {
        return MarkupDocument.escapeNodeData(this._data);
    }

    get data() { return this._data; }
    set data(newData) { this._data = newData.toString(); }
}

const componentsMap = new Map;
const componentsByClass = new Map;
const componentsToRender = new Set;
let currentlyRenderingComponent = null;
class MarkupComponentBase extends CommonComponentBase {
    constructor(name)
    {
        super();
        this._name = componentsByClass.get(new.target);
        const component = componentsMap.get(this._name);
        console.assert(component, `Component "${this._name}" has not been defined`);
        this._componentId = component.id;
        this._element = null;
        this._contentRoot = null;
    }

    element()
    {
        if (!this._element) {
            this._element = MarkupDocument.createElement(this._name);
            this._element.component = () => this;
        }
        return this._element;
    }

    content(id = null)
    {
        this._ensureContentTree();
        if (id) {
            // FIXME: Make this more efficient.
            return this._contentRoot ? this._findElementRecursivelyById(this._contentRoot, id) : null;
        }
        return this._contentRoot;
    }

    _findElementRecursivelyById(parent, id)
    {
        for (const child of parent.childNodes) {
            if (child.id == id)
                return child;
            if (child instanceof MarkupParentNode) {
                const result = this._findElementRecursivelyById(child, id);
                if (result)
                    return result;
            }
        }
        return null;
    }

    render() { this._ensureContentTree(); }

    enqueueToRender()
    {
        componentsToRender.add(this);
    }

    static runRenderLoop()
    {
        console.assert(!currentlyRenderingComponent);
        do {
            const currentSet = [...componentsToRender];
            componentsToRender.clear();
            for (let component of currentSet) {
                const enqueuedAgain = componentsToRender.has(component);
                if (enqueuedAgain)
                    continue;
                currentlyRenderingComponent = component;
                component.render();
            }
            currentlyRenderingComponent = null;
        } while (componentsToRender.size);
    }

    renderReplace(parentNode, content)
    {
        console.assert(currentlyRenderingComponent == this);
        console.assert(parentNode instanceof MarkupParentNode);
        parentNode.removeAllChildren();
        if (content) {
            MarkupComponentBase._addContentToElement(parentNode, content);

            const component = componentsMap.get(this._name);
            console.assert(component);
            this._applyStyleOverrides(parentNode, component.styleOverride);
        }
    }

    _applyStyleOverrides(node, styleOverride)
    {
        if (node instanceof MarkupElement)
            styleOverride(node);
        if (node.childNodes) {
            for (const child of node.childNodes)
                this._applyStyleOverrides(child, styleOverride);
        }
    }

    _ensureContentTree()
    {
        if (this._contentRoot)
            return;

        const thisClass = this.__proto__.constructor;
        const component = componentsMap.get(this._name);
        if (!component.parsed) {
            component.parsed = true;

            const htmlTemplate = thisClass['htmlTemplate'];
            const cssTemplate = thisClass['cssTemplate'];
            if (htmlTemplate || cssTemplate)
                throw 'The operation is not supported';

            const contentTemplate = thisClass['contentTemplate'];
            const styleTemplate = thisClass['styleTemplate'];
            if (!contentTemplate && !styleTemplate)
                return;

            const result = MarkupComponentBase._parseTemplates(this._name, this._componentId, contentTemplate, styleTemplate);
            component.content = result.content;
            component.stylesheet = result.stylesheet;
            component.styleOverride = result.styleOverride;
        }

        this._contentRoot = MarkupDocument.createContentRoot(this.element());
        if (component.content) {
            for (const node of component.content)
                this._contentRoot.appendChild(node.clone());
            this._recursivelyUpgradeUnknownElements(this._contentRoot,
                (node) => {
                    const component = node instanceof MarkupElement ? componentsMap.get(node.localName) : null;
                    return component ? component.class : null;
                },
                (component) => component.enqueueToRender());
        }
        // FIXME: Add a call to didConstructShadowTree.
    }

    static reset()
    {
        console.assert(!currentlyRenderingComponent);
        MarkupDocument.reset();
        componentsMap.clear();
        componentsByClass.clear();
        componentsToRender.clear();
    }

    static _parseTemplates(componentName, componentId, contentTemplate, styleTemplate)
    {
        const styledClasses = new Map;
        const styledElements = new Map;
        let stylesheet = null;
        let selectorId = 0;
        let content = null;
        let styleOverride = () => { }
        if (styleTemplate) {
            stylesheet = this._constructStylesheetFromTemplate(styleTemplate, (selector, rule) => {
                if (selector == ':host')
                    return componentName;

                const match = selector.match(/^(\.?[a-zA-Z0-9\-]+)(\[[a-zA-Z0-9\-]+\]|\:[a-z\-]+)*$/);
                if (!match)
                    throw 'Unsupported selector: ' + selector;

                const selectorSuffix = match[2] || '';
                let globalClassName;
                // FIXME: Preserve the specificity of selectors.
                selectorId++;
                if (match[1].startsWith('.')) {
                    const className = match[1].substring(1);
                    globalClassName = `content-${componentId}-class-${className}`;
                    styledClasses.set(className, globalClassName);
                    return '.' + globalClassName + selectorSuffix;
                }

                const elementName = match[1].toLowerCase();
                globalClassName = `content-${componentId}-element-${elementName}`;
                styledElements.set(elementName, globalClassName);
                return '.' + globalClassName + selectorSuffix;
            });

            if (styledClasses.size || styledElements.size) {
                styleOverride = (element) => {
                    const classNamesToAdd = new Set;
                    const globalClassNameForName = styledElements.get(element.localName);
                    if (globalClassNameForName)
                        classNamesToAdd.add(globalClassNameForName);

                    const currentClass = element.getAttribute('class');
                    if (currentClass) {
                        const classList = currentClass.split(/\s+/);
                        for (const className of classList) {
                            const globalClass = styledClasses.get(className);
                            if (globalClass)
                                classNamesToAdd.add(globalClass);
                            classNamesToAdd.add(className);
                        }
                        if (classList.length == classNamesToAdd.size)
                            return;
                    } else if (!classNamesToAdd.size)
                        return;
                    element.setAttribute('class', Array.from(classNamesToAdd).join(' '));
                }
            }
        }

        if (contentTemplate)
            content = MarkupComponentBase._constructNodeTreeFromTemplate(contentTemplate, styleOverride);

        return {stylesheet, content, styleOverride};
    }

    static defineElement(name, componentClass)
    {
        console.assert(!componentsMap.get(name), `The component "${name}" has already been defined`);
        const existingComponentForClass = componentsByClass.get(componentClass);
        console.assert(!existingComponentForClass, existingComponentForClass
            ? `The component class "${existingComponentForClass}" has already been used to define another component "${existingComponentForClass.name}"` : '');
        componentsMap.set(name, {
            class: componentClass,
            id: componentsMap.size + 1,
            parsed: false,
            content: null,
            stylesheet: null,
        });
        componentsByClass.set(componentClass, name);
    }

    createEventHandler(callback) { return MarkupComponentBase.createEventHandler(callback); }
    static createEventHandler(callback)
    {
        throw 'The operation is not supported';
    }
}
CommonComponentBase._context = MarkupDocument;
CommonComponentBase._isNode = (node) => node instanceof MarkupNode;
CommonComponentBase._baseClass = MarkupComponentBase;

class MarkupPage extends MarkupComponentBase {
    constructor(title)
    {
        super('page-component');
        this._title = title;
        this._updateComponentsStylesheetLazily = new LazilyEvaluatedFunction(this._updateComponentsStylesheet.bind(this));
    }

    pageTitle() { return this._title; }

    content(id)
    {
        if (id)
            return super.content(id);
        return super.content('page-body');
    }

    render()
    {
        super.render();
        this.content('page-title').textContent = this.pageTitle();
        this._updateComponentsStylesheetLazily.evaluate([...componentsMap.values()].filter((component) => component.parsed && component.stylesheet));
    }

    _updateComponentsStylesheet(componentsWithStylesheets)
    {
        let mergedStylesheetText = '';
        for (const component of componentsWithStylesheets)
            mergedStylesheetText += component.stylesheet;
        this.content('component-style-rules').textContent = mergedStylesheetText;
    }

    static get contentTemplate()
    {
        return ['html', [
            ['head', [
                ['title', {id: 'page-title'}],
                ['style', {id: 'component-style-rules'}]
            ]],
            ['body', {id: 'page-body'}, this.pageContent]
        ]];
    }

    generateMarkup()
    {
        this.enqueueToRender(this);
        MarkupComponentBase.runRenderLoop();
        this.enqueueToRender(this);
        MarkupComponentBase.runRenderLoop();
        return '<!DOCTYPE html>' + MarkupDocument.markup(super.content());
    }

}
MarkupComponentBase.defineElement('page-component', MarkupPage);

if (typeof module != 'undefined') {
    module.exports.MarkupDocument = MarkupDocument;
    module.exports.MarkupComponentBase = MarkupComponentBase;
    module.exports.MarkupPage = MarkupPage;
}
