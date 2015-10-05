window.Utilities =
{
    _parse: function(str, sep)
    {
        var output = {};
        str.split(sep).forEach(function(part) {
            var item = part.split("=");
            var value = decodeURIComponent(item[1]);
            if (value[0] == "'" )
                output[item[0]] = value.substr(1, value.length - 2);
            else
                output[item[0]] = value;                
          });
        return output;
    },
    
    parseParameters: function()
    {
        return this._parse(window.location.search.substr(1), "&");
    },
    
    parseArguments: function(str)
    {
        return this._parse(str, " ");
    },
    
    extendObject: function(obj1, obj2)
    {
        for (var attrname in obj2)
            obj1[attrname] = obj2[attrname];
        return obj1;
    },
    
    copyObject: function(obj)
    {
        return this.extendObject({}, obj);
    },
    
    mergeObjects: function(obj1, obj2)
    {
        return this.extendObject(this.copyObject(obj1), obj2);
    },
    
    createSvgElement: function(name, attrs, xlinkAttrs, parent)
    {
        const svgNamespace = "http://www.w3.org/2000/svg";
        const xlinkNamespace = "http://www.w3.org/1999/xlink";

        var element = document.createElementNS(svgNamespace, name);
        
        for (var key in attrs)
            element.setAttribute(key, attrs[key]);
            
        for (var key in xlinkAttrs)
            element.setAttributeNS(xlinkNamespace, key, xlinkAttrs[key]);
            
        parent.appendChild(element);
        return element;
    }
}
