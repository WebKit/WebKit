/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

WebInspector.StylesSidebarPane = function()
{
    WebInspector.SidebarPane.call(this, "Styles");
}

WebInspector.StylesSidebarPane.prototype = {
    update: function(node)
    {
        var body = this.bodyElement;

        body.removeChildren();

        this.sections = [];

        if (!node)
            return;

        if (node.nodeType === Node.TEXT_NODE && node.parentNode)
            node = node.parentNode;

        var styleRules = [];
        var styleProperties = [];

        if (node.nodeType === Node.ELEMENT_NODE) {
            var propertyCount = [];

            var computedStyle = node.ownerDocument.defaultView.getComputedStyle(node);
            if (computedStyle && computedStyle.length)
                styleRules.push({ computedStyle: true, selectorText: "Computed Style", style: computedStyle });

            var nodeName = node.nodeName.toLowerCase();
            for (var i = 0; i < node.attributes.length; ++i) {
                var attr = node.attributes[i];
                if (attr.style) {
                    var attrStyle = { style: attr.style };
                    attrStyle.subtitle = "element\u2019s \u201C" + attr.name + "\u201D attribute";
                    attrStyle.selectorText = nodeName + "[" + attr.name;
                    if (attr.value.length)
                        attrStyle.selectorText += "=" + attr.value;
                    attrStyle.selectorText += "]";
                    styleRules.push(attrStyle);
                }
            }

            if (node.style && node.style.length) {
                var inlineStyle = { selectorText: "Inline Style Attribute", style: node.style };
                inlineStyle.subtitle = "element\u2019s \u201Cstyle\u201D attribute";
                styleRules.push(inlineStyle);
            }

            var matchedStyleRules = node.ownerDocument.defaultView.getMatchedCSSRules(node, "", !Preferences.showUserAgentStyles);
            if (matchedStyleRules) {
                // Add rules in reverse order to match the cascade order.
                for (var i = (matchedStyleRules.length - 1); i >= 0; --i)
                    styleRules.push(matchedStyleRules[i]);
            }

            var usedProperties = {};
            var priorityUsed = false;

            // Walk the style rules and make a list of all used and overloaded properties.
            for (var i = 0; i < styleRules.length; ++i) {
                if (styleRules[i].computedStyle)
                    continue;

                styleRules[i].overloadedProperties = {};

                var foundProperties = {};

                var style = styleRules[i].style;
                for (var j = 0; j < style.length; ++j) {
                    var name = style[j];
                    var shorthand = style.getPropertyShorthand(name);
                    var overloaded = (name in usedProperties);

                    if (!priorityUsed && style.getPropertyPriority(name).length)
                        priorityUsed = true;

                    if (overloaded)
                        styleRules[i].overloadedProperties[name] = true;

                    foundProperties[name] = true;
                    if (shorthand)
                        foundProperties[shorthand] = true;

                    if (name === "font") {
                        // The font property is not reported as a shorthand. Report finding the individual
                        // properties so they are visible in computed style.
                        // FIXME: remove this when http://bugs.webkit.org/show_bug.cgi?id=15598 is fixed.
                        foundProperties["font-family"] = true;
                        foundProperties["font-size"] = true;
                        foundProperties["font-style"] = true;
                        foundProperties["font-variant"] = true;
                        foundProperties["font-weight"] = true;
                        foundProperties["line-height"] = true;
                    }
                }

                // Add all the properties found in this style to the used properties list.
                // Do this here so only future rules are affect by properties used in this rule.
                for (var name in foundProperties)
                    usedProperties[name] = true;
            }

            if (priorityUsed) {
                // Walk the properties again and account for !important.
                var foundPriorityProperties = [];

                // Walk in reverse to match the order !important overrides.
                for (var i = (styleRules.length - 1); i >= 0; --i) {
                    if (styleRules[i].computedStyle)
                        continue;

                    var foundProperties = {};
                    var style = styleRules[i].style;
                    for (var j = 0; j < style.length; ++j) {
                        var name = style[j];

                        // Skip duplicate properties in the same rule.
                        if (name in foundProperties)
                            continue;

                        foundProperties[name] = true;

                        if (style.getPropertyPriority(name).length) {
                            if (!(name in foundPriorityProperties))
                                delete styleRules[i].overloadedProperties[name];
                            else
                                styleRules[i].overloadedProperties[name] = true;
                            foundPriorityProperties[name] = true;
                        } else if (name in foundPriorityProperties)
                            styleRules[i].overloadedProperties[name] = true;
                    }
                }
            }

            // Make a property section for each style rule.
            var styleRulesLength = styleRules.length;
            for (var i = 0; i < styleRulesLength; ++i) {
                var styleRule = styleRules[i];
                var subtitle = styleRule.subtitle;
                delete styleRule.subtitle;

                var computedStyle = styleRule.computedStyle;
                delete styleRule.computedStyle;

                var overloadedProperties = styleRule.overloadedProperties;
                delete styleRule.overloadedProperties;

                var section = new WebInspector.StylePropertiesSection(styleRule, subtitle, computedStyle, (overloadedProperties || usedProperties));
                section.expanded = true;

                body.appendChild(section.element);
                this.sections.push(section);
            }
        } else {
            // can't style this node
        }
    }
}

WebInspector.StylesSidebarPane.prototype.__proto__ = WebInspector.SidebarPane.prototype;

WebInspector.StylePropertiesSection = function(styleRule, subtitle, computedStyle, overloadedOrUsedProperties)
{
    WebInspector.PropertiesSection.call(this, styleRule.selectorText);

    this.styleRule = styleRule;
    this.computedStyle = computedStyle;

    if (computedStyle)
        this.usedProperties = overloadedOrUsedProperties;
    else
        this.overloadedProperties = overloadedOrUsedProperties || {};

    if (computedStyle) {
        if (Preferences.showInheritedComputedStyleProperties)
            this.element.addStyleClass("show-inherited");

        var showInheritedLabel = document.createElement("label");
        var showInheritedInput = document.createElement("input");
        showInheritedInput.type = "checkbox";
        showInheritedInput.checked = Preferences.showInheritedComputedStyleProperties;

        var computedStyleSection = this;
        var showInheritedToggleFunction = function(event) {
            Preferences.showInheritedComputedStyleProperties = showInheritedInput.checked;
            if (Preferences.showInheritedComputedStyleProperties)
                computedStyleSection.element.addStyleClass("show-inherited");
            else
                computedStyleSection.element.removeStyleClass("show-inherited");
            event.stopPropagation();
        };

        showInheritedLabel.addEventListener("click", showInheritedToggleFunction, false);

        showInheritedLabel.appendChild(showInheritedInput);
        showInheritedLabel.appendChild(document.createTextNode("Show inherited properties"));
        this.subtitleElement.appendChild(showInheritedLabel);
    } else {
        if (!subtitle) {
            if (this.styleRule.parentStyleSheet && this.styleRule.parentStyleSheet.href) {
                var url = this.styleRule.parentStyleSheet.href;
                subtitle = WebInspector.linkifyURL(url, url.trimURL(WebInspector.mainResource.domain).escapeHTML());
                this.subtitleElement.addStyleClass("file");
            } else if (this.styleRule.parentStyleSheet && !this.styleRule.parentStyleSheet.ownerNode)
                subtitle = "user agent stylesheet";
            else
                subtitle = "inline stylesheet";
        }

        this.subtitle = subtitle;
    }
}

WebInspector.StylePropertiesSection.prototype = {
    onpopulate: function()
    {
        var style = this.styleRule.style;
        if (!style.length)
            return;

        var foundProperties = {};

        // Add properties in reverse order to better match how the style
        // system picks the winning value for duplicate properties.
        for (var i = (style.length - 1); i >= 0; --i) {
            var name = style[i];
            var shorthand = style.getPropertyShorthand(name);

            if (name in foundProperties || (shorthand && shorthand in foundProperties))
                continue;

            foundProperties[name] = true;
            if (shorthand)
                foundProperties[shorthand] = true;

            if (this.computedStyle)
                var inherited = (this.usedProperties && !((shorthand || name) in this.usedProperties));
            else {
                var overloaded = ((shorthand || name) in this.overloadedProperties);

                if (shorthand && !overloaded) {
                    // Find out if all the individual properties of a shorthand
                    // are overloaded and mark the shorthand as overloaded too.

                    var count = 0;
                    var overloadCount = 0;
                    for (var j = 0; j < style.length; ++j) {
                        var individualProperty = style[j];
                        if (style.getPropertyShorthand(individualProperty) !== shorthand)
                            continue;
                        ++count;
                        if (individualProperty in this.overloadedProperties)
                            ++overloadCount;
                    }

                    overloaded = (overloadCount >= count);
                }
            }

            var item = new WebInspector.StylePropertyTreeElement(style, (shorthand || name), this.computedStyle, (shorthand ? true : false), (overloaded || inherited));
            this.propertiesTreeOutline.insertChild(item, 0);
        }
    }
}

WebInspector.StylePropertiesSection.prototype.__proto__ = WebInspector.PropertiesSection.prototype;

WebInspector.StylePropertyTreeElement = function(style, name, computedStyle, shorthand, overloadedOrInherited)
{
    // These properties should always show for Computed Style
    var alwaysShowComputedProperties = { "display": true, "height": true, "width": true };

    // "Nicknames" for some common values that are easier to read.
    var valueNicknames = {
        "rgb(0, 0, 0)": "black",
        "#000": "black",
        "#000000": "black",
        "rgb(255, 255, 255)": "white",
        "#fff": "white",
        "#ffffff": "white",
        "#FFF": "white",
        "#FFFFFF": "white",
        "rgba(0, 0, 0, 0)": "transparent",
        "rgb(255, 0, 0)": "red",
        "rgb(0, 255, 0)": "lime",
        "rgb(0, 0, 255)": "blue",
        "rgb(255, 255, 0)": "yellow",
        "rgb(255, 0, 255)": "magenta",
        "rgb(0, 255, 255)": "cyan"
    };

    this.style = style;
    this.name = name;
    this.computedStyle = computedStyle;
    this.shorthand = shorthand;
    this.overloaded = (!computedStyle && overloadedOrInherited);
    this.inherited = (computedStyle && overloadedOrInherited && !(name in alwaysShowComputedProperties));

    var priority = style.getPropertyPriority(name);
    var value = style.getPropertyValue(name);
    var htmlValue = value;

    if (priority && !priority.length)
        delete priority;

    if (!priority && shorthand) {
        // Priority is not returned for shorthands, find the priority from an individual property.
        for (var i = 0; i < style.length; ++i) {
            var individualProperty = style[i];
            if (style.getPropertyShorthand(individualProperty) !== name)
                continue;
            priority = style.getPropertyPriority(individualProperty);
            break;
        }
    }

    if (value) {
        var urls = value.match(/url\([^)]+\)/);
        if (urls) {
            for (var i = 0; i < urls.length; ++i) {
                var url = urls[i].substring(4, urls[i].length - 1);
                htmlValue = htmlValue.replace(urls[i], "url(" + WebInspector.linkifyURL(url) + ")");
            }
        } else {
            if (value in valueNicknames)
                htmlValue = valueNicknames[value];
            htmlValue = htmlValue.escapeHTML();
        }
    } else if (shorthand) {
        // Some shorthands (like border) return a null value, so compute a shorthand value.
        // FIXME: remove this when http://bugs.webkit.org/show_bug.cgi?id=15823 is fixed.

        value = "";

        var foundProperties = {};
        for (var i = 0; i < style.length; ++i) {
            var individualProperty = style[i];
            if (style.getPropertyShorthand(individualProperty) !== name || individualProperty in foundProperties)
                continue;

            var individualValue = style.getPropertyValue(individualProperty);
            if (style.isPropertyImplicit(individualProperty) || individualValue === "initial")
                continue;

            foundProperties[individualProperty] = true;

            if (value.length)
                value += " ";
            value += individualValue;
        }

        htmlValue = value.escapeHTML();
    } else
        htmlValue = value = "";

    var classes = [];
    if (!computedStyle && (style.isPropertyImplicit(name) || value === "initial"))
        classes.push("implicit");
    if (this.inherited)
        classes.push("inherited");
    if (this.overloaded)
        classes.push("overloaded");

    var title = "";
    if (classes.length)
        title += "<span class=\"" + classes.join(" ") + "\">";

    title += "<span class=\"name\">" + name.escapeHTML() + "</span>: ";
    title += "<span class=\"value\">" + htmlValue;
    if (priority)
        title += " !" + priority;
    title += "</span>;";

    if (value) {
        // FIXME: this dosen't catch keyword based colors like black and white
        var colors = value.match(/((rgb|hsl)a?\([^)]+\))|(#[0-9a-fA-F]{6})|(#[0-9a-fA-F]{3})/g);
        if (colors) {
            var colorsLength = colors.length;
            for (var i = 0; i < colorsLength; ++i)
                title += "<span class=\"swatch\" style=\"background-color: " + colors[i] + "\"></span>";
        }
    }

    if (classes.length)
        title += "</span>";

    TreeElement.call(this, title, null, shorthand);

    this.tooltip = name + ": " + (valueNicknames[value] || value) + (priority ? " !" + priority : "");
}

WebInspector.StylePropertyTreeElement.prototype = {
    onpopulate: function()
    {
        // Only populate once and if this property is a shorthand.
        if (this.children.length || !this.shorthand)
            return;

        var foundProperties = {};

        // Add properties in reverse order to better match how the style
        // system picks the winning value for duplicate properties.
        for (var i = (this.style.length - 1); i >= 0; --i) {
            var name = this.style[i];
            var shorthand = this.style.getPropertyShorthand(name);

            if (shorthand !== this.name || name in foundProperties)
                continue;

            foundProperties[name] = true;

            if (this.computedStyle)
                var inherited = (this.treeOutline.section.usedProperties && !(name in this.treeOutline.section.usedProperties));
            else
                var overloaded = (name in this.treeOutline.section.overloadedProperties);

            var item = new WebInspector.StylePropertyTreeElement(this.style, name, this.computedStyle, false, (inherited || overloaded));
            this.insertChild(item, 0);
        }
    }
}

WebInspector.StylePropertyTreeElement.prototype.__proto__ = TreeElement.prototype;
