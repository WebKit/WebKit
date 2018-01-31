
const dirtyNodes = new Set;
const nodesRequiringChildrenUpdate = new Set;

class LayoutNode
{

    constructor(stringOrElement)
    {

        if (!stringOrElement)
            this.element = document.createElement("div");
        else if (stringOrElement instanceof Element)
            this.element = stringOrElement;
        else if (typeof stringOrElement === "string" || stringOrElement instanceof String)
            this.element = elementFromString(stringOrElement);

        this._parent = null;
        this._children = [];

        this._x = 0;
        this._y = 0;
        this._width = 0;
        this._height = 0;
        this._visible = true;

        this._needsLayout = false;
        this._dirtyProperties = new Set;

        this._pendingDOMManipulation = LayoutNode.DOMManipulation.None;
    }

    // Public

    get x()
    {
        return this._x;
    }

    set x(x)
    {
        if (x === this._x)
            return;

        this._x = x;
        this.markDirtyProperty("x");
    }

    get y()
    {
        return this._y;
    }

    set y(y)
    {
        if (y === this._y)
            return;

        this._y = y;
        this.markDirtyProperty("y");
    }

    get width()
    {
        return this._width;
    }

    set width(width)
    {
        if (width === this._width)
            return;

        this._width = width;
        this.markDirtyProperty("width");
        this.layout();
    }

    get height()
    {
        return this._height;
    }

    set height(height)
    {
        if (height === this._height)
            return;

        this._height = height;
        this.markDirtyProperty("height");
        this.layout();
    }

    get visible()
    {
        return this._visible;
    }

    set visible(flag)
    {
        if (flag === this._visible)
            return;

        this._visible = flag;
        this.markDirtyProperty("visible");
    }

    get needsLayout()
    {
        return this._needsLayout || this._pendingDOMManipulation !== LayoutNode.DOMManipulation.None || this._dirtyProperties.size > 0;
    }

    set needsLayout(flag)
    {
        if (this.needsLayout === flag)
            return;

        this._needsLayout = flag;
        this._updateDirtyState();
    }

    get parent()
    {
        return this._parent;
    }

    get children()
    {
        return this._children;
    }

    set children(children)
    {
        if (children.length === this._children.length) {
            let arraysDiffer = false;
            for (let i = children.length - 1; i >= 0; --i) {
                if (children[i] !== this._children[i]) {
                    arraysDiffer = true;
                    break;
                }
            }
            if (!arraysDiffer)
                return;
        }

        this._updatingChildren = true;

        while (this._children.length)
            this.removeChild(this._children[0]);

        for (let child of children)
            this.addChild(child);

        delete this._updatingChildren;
        this.didChangeChildren();
    }

    parentOfType(type)
    {
        let node = this;
        while (node = node._parent) {
            if (node instanceof type)
                return node;
        }
        return null;
    }

    addChild(child, index)
    {
        child.remove();

        if (index === undefined || index < 0 || index > this._children.length)
            index = this._children.length;

        this._children.splice(index, 0, child);
        child._parent = this;

        if (!this._updatingChildren)
            this.didChangeChildren();

        child._markNodeManipulation(LayoutNode.DOMManipulation.Addition);

        return child;
    }

    insertBefore(newSibling, referenceSibling)
    {
        return this.addChild(newSibling, this._children.indexOf(referenceSibling));
    }

    insertAfter(newSibling, referenceSibling)
    {
        const index = this._children.indexOf(referenceSibling);
        return this.addChild(newSibling, index + 1);
    }

    removeChild(child)
    {
        if (child._parent !== this)
            return;

        const index = this._children.indexOf(child);
        if (index === -1)
            return;

        this.willRemoveChild(child);
        this._children.splice(index, 1);
        child._parent = null;

        if (!this._updatingChildren)
            this.didChangeChildren();

        child._markNodeManipulation(LayoutNode.DOMManipulation.Removal);

        return child;
    }

    remove()
    {
        if (this._parent instanceof LayoutNode)
            return this._parent.removeChild(this);
    }

    markDirtyProperty(propertyName)
    {
        const hadProperty = this._dirtyProperties.has(propertyName);
        this._dirtyProperties.add(propertyName);

        if (!hadProperty)
            this._updateDirtyState();
    }

    // Protected

    layout()
    {
        // Implemented by subclasses.
    }

    commit()
    {
        if (this._pendingDOMManipulation === LayoutNode.DOMManipulation.Removal) {
            const parent = this.element.parentNode;
            if (parent)
                parent.removeChild(this.element);
        }
    
        for (let propertyName of this._dirtyProperties)
            this.commitProperty(propertyName);

        this._dirtyProperties.clear();

        if (this._pendingDOMManipulation === LayoutNode.DOMManipulation.Addition)
            nodesRequiringChildrenUpdate.add(this.parent);
    }

    commitProperty(propertyName)
    {
        const style = this.element.style;

        switch (propertyName) {
        case "x":
            style.left = `${this._x}px`;
            break;
        case "y":
            style.top = `${this._y}px`;
            break;
        case "width":
            style.width = `${this._width}px`;
            break;
        case "height":
            style.height = `${this._height}px`;
            break;
        case "visible":
            if (this._visible)
                style.removeProperty("display");
            else
                style.display = "none";
            break;
        }
    }

    willRemoveChild(child)
    {
        // Implemented by subclasses.
    }

    didChangeChildren()
    {
        // Implemented by subclasses.
    }

    // Private

    _markNodeManipulation(manipulation)
    {
        this._pendingDOMManipulation = manipulation;
        this._updateDirtyState();
    }

    _updateDirtyState()
    {
        if (this.needsLayout) {
            dirtyNodes.add(this);
            scheduler.scheduleLayout(performScheduledLayout);
        } else {
            dirtyNodes.delete(this);
            if (dirtyNodes.size === 0)
                scheduler.unscheduleLayout(performScheduledLayout);
        }
    }

    _updateChildren()
    {
        let nextChildElement = null;
        const element = this.element;
        for (let i = this.children.length - 1; i >= 0; --i) {
            let child = this.children[i];
            let childElement = child.element;

            if (child._pendingDOMManipulation === LayoutNode.DOMManipulation.Addition) {
                element.insertBefore(childElement, nextChildElement);
                child._pendingDOMManipulation = LayoutNode.DOMManipulation.None;
            }

            nextChildElement = childElement;
        }
    }

}

LayoutNode.DOMManipulation = {
    None:     0,
    Removal:  1,
    Addition: 2
};

function performScheduledLayout()
{
    const previousDirtyNodes = Array.from(dirtyNodes);
    dirtyNodes.clear();
    previousDirtyNodes.forEach(node => {
        node._needsLayout = false;
        node.layout();
        node.commit();
    });

    nodesRequiringChildrenUpdate.forEach(node => node._updateChildren());
    nodesRequiringChildrenUpdate.clear();
}

function elementFromString(elementString)
{
    const element = document.createElement("div");
    element.innerHTML = elementString;
    return element.firstElementChild;
}
