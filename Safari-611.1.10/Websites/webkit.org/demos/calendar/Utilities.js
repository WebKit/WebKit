// Element extension --------------------------------------------------------------

Element.prototype.removeStyleClass = function(className) 
{
    // Test for the simple case before using a RegExp.
    if (this.className === className) {
        this.className = "";
        return;
    }

    var regex = new RegExp("(^|\\s+)" + className.escapeForRegExp() + "($|\\s+)");
    if (regex.test(this.className))
        this.className = this.className.replace(regex, " ");
}

Element.prototype.addStyleClass = function(className) 
{
    if (className && !this.hasStyleClass(className))
        this.className += (this.className.length ? " " + className : className);
}

Element.prototype.hasStyleClass = function(className) 
{
    if (!className)
        return false;
    // Test for the simple case before using a RegExp.
    if (this.className === className)
        return true;
    var regex = new RegExp("(^|\\s)" + className.escapeForRegExp() + "($|\\s)");
    return regex.test(this.className);
}

Element.prototype.removeChildren = function()
{
    while (this.firstChild) 
        this.removeChild(this.firstChild);        
}

Element.prototype.findParentOfTagName = function(tagName)
{
    var parent = this;
    while (parent) {
        if (parent.tagName == tagName)
            return parent;
        parent = parent.parentNode;
    }
    return null;
}

Element.prototype.__defineGetter__("totalOffsetLeft", function()
{
    var total = 0;
    for (var element = this; element; element = element.offsetParent)
        total += element.offsetLeft;
    return total;
});

Element.prototype.__defineGetter__("totalOffsetTop", function()
{
    var total = 0;
    for (var element = this; element; element = element.offsetParent)
        total += element.offsetTop;
    return total;
});

// String extension ------------------------------------------------------

String.prototype.hasSubstring = function(string, caseInsensitive)
{
    if (!caseInsensitive)
        return this.indexOf(string) !== -1;
    return this.match(new RegExp(string.escapeForRegExp(), "i"));
}

String.prototype.escapeCharacters = function(chars)
{
    var foundChar = false;
    for (var i = 0; i < chars.length; ++i) {
        if (this.indexOf(chars.charAt(i)) !== -1) {
            foundChar = true;
            break;
        }
    }

    if (!foundChar)
        return this;

    var result = "";
    for (var i = 0; i < this.length; ++i) {
        if (chars.indexOf(this.charAt(i)) !== -1)
            result += "\\";
        result += this.charAt(i);
    }

    return result;
}

String.prototype.escapeForRegExp = function()
{
    return this.escapeCharacters("^[]{}()\\.$*+?|");
}

// Number extension ------------------------------------------------------

Number.constrain = function(num, min, max)
{
    if (num < min)
        num = min;
    else if (num > max)
        num = max;
    return num;
}

// Date extension --------------------------------------------------------

Date.prototype.isToday = function()
{
    var today = new Date();
    return (today.getFullYear() == this.getFullYear() && today.getMonth() == this.getMonth() && today.getDate() == this.getDate());    
}

Date.prototype.roundToHour = function() {
    if (this.getMinutes() == 0)
        return; // Already rounded to the hour
    if (this.getHours() < 23)
        this.setHours(this.getHours() + 1);
    this.setMinutes(0);
}

Date.dayDateFromTime = function(time) {
    var date = new Date(time);
    date.setHours(0, 0, 0, 0);
    return date;
}
