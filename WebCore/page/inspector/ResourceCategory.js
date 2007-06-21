WebInspector.ResourceCategory = function(title)
{
    this.title = title;
    this.resources = [];
    this.listItem = new WebInspector.ResourceCategoryTreeElement(this);
    this.listItem.hidden = true;
    WebInspector.fileOutline.appendChild(this.listItem);
}

WebInspector.ResourceCategory.prototype = {
    toString: function()
    {
        return this.title;
    },

    addResource: function(resource)
    {
        var a = resource;
        var resourcesLength = this.resources.length;
        for (var i = 0; i < resourcesLength; ++i) {
            var b = this.resources[i];
            if (a._lastPathComponentLowerCase < b._lastPathComponentLowerCase)
                break;
        }

        this.resources.splice(i, 0, resource);
        this.listItem.insertChild(resource.listItem, i);
        this.listItem.hidden = false;

        resource.attach();
    },

    removeResource: function(resource)
    {
        resource.detach();

        var resourcesLength = this.resources.length;
        for (var i = 0; i < resourcesLength; ++i) {
            if (this.resources[i] === resource) {
                this.resources.splice(i, 1);
                break;
            }
        }

        this.listItem.removeChild(resource.listItem);

        if (!this.resources.length)
            this.listItem.hidden = true;
    },

    removeAllResources: function(resource)
    {
        var resourcesLength = this.resources.length;
        for (var i = 0; i < resourcesLength; ++i)
            this.resources[i].detach();
        this.resources = [];
        this.listItem.removeChildren();
        this.listItem.hidden = true;
    }
}

WebInspector.ResourceCategoryTreeElement = function(category)
{
    var item = new TreeElement(category.title, category, true);
    item.selectable = false;
    return item;
}
