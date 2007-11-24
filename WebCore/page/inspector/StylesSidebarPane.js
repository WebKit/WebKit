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
    WebInspector.SidebarPane.call(this, WebInspector.UIString("Styles"));
}

WebInspector.StylesSidebarPane.prototype = {
    update: function(node, editedSection)
    {
        var refresh = false;

        if (!node || node === this.node)
            refresh = true;

        if (node && node.nodeType === Node.TEXT_NODE && node.parentNode)
            node = node.parentNode;

        if (node && node.nodeType !== Node.ELEMENT_NODE)
            node = null;

        if (node)
            this.node = node;
        else
            node = this.node;

        var body = this.bodyElement;
        if (!refresh || !node) {
            body.removeChildren();
            this.sections = [];
        }

        if (!node)
            return;

        var styleRules = [];

        if (refresh) {
            for (var i = 0; i < this.sections.length; ++i) {
                var section = this.sections[i];
                if (section.computedStyle)
                    section.styleRule.style = node.ownerDocument.defaultView.getComputedStyle(node);
                var styleRule = { section: section, style: section.styleRule.style, computedStyle: section.computedStyle };
                styleRules.push(styleRule);
            }
        } else {
            var computedStyle = node.ownerDocument.defaultView.getComputedStyle(node);
            styleRules.push({ computedStyle: true, selectorText: WebInspector.UIString("Computed Style"), style: computedStyle, editable: false });

            var nodeName = node.nodeName.toLowerCase();
            for (var i = 0; i < node.attributes.length; ++i) {
                var attr = node.attributes[i];
                if (attr.style) {
                    var attrStyle = { style: attr.style, editable: false };
                    attrStyle.subtitle = WebInspector.UIString("element’s “%s” attribute", attr.name);
                    attrStyle.selectorText = nodeName + "[" + attr.name;
                    if (attr.value.length)
                        attrStyle.selectorText += "=" + attr.value;
                    attrStyle.selectorText += "]";
                    styleRules.push(attrStyle);
                }
            }

            if (node.style && node.style.length) {
                var inlineStyle = { selectorText: WebInspector.UIString("Inline Style Attribute"), style: node.style };
                inlineStyle.subtitle = WebInspector.UIString("element’s “%s” attribute", "style");
                styleRules.push(inlineStyle);
            }

            var matchedStyleRules = node.ownerDocument.defaultView.getMatchedCSSRules(node, "", !Preferences.showUserAgentStyles);
            if (matchedStyleRules) {
                // Add rules in reverse order to match the cascade order.
                for (var i = (matchedStyleRules.length - 1); i >= 0; --i)
                    styleRules.push(matchedStyleRules[i]);
            }
        }

        var usedProperties = {};
        var priorityUsed = false;

        // Walk the style rules and make a list of all used and overloaded properties.
        for (var i = 0; i < styleRules.length; ++i) {
            var styleRule = styleRules[i];
            if (styleRule.computedStyle)
                continue;

            styleRule.usedProperties = {};

            var style = styleRule.style;
            for (var j = 0; j < style.length; ++j) {
                var name = style[j];

                if (!priorityUsed && style.getPropertyPriority(name).length)
                    priorityUsed = true;

                // If the property name is already used by another rule then this rule's
                // property is overloaded, so don't add it to the rule's usedProperties.
                if (!(name in usedProperties))
                    styleRule.usedProperties[name] = true;

                if (name === "font") {
                    // The font property is not reported as a shorthand. Report finding the individual
                    // properties so they are visible in computed style.
                    // FIXME: remove this when http://bugs.webkit.org/show_bug.cgi?id=15598 is fixed.
                    styleRule.usedProperties["font-family"] = true;
                    styleRule.usedProperties["font-size"] = true;
                    styleRule.usedProperties["font-style"] = true;
                    styleRule.usedProperties["font-variant"] = true;
                    styleRule.usedProperties["font-weight"] = true;
                    styleRule.usedProperties["line-height"] = true;
                }
            }

            // Add all the properties found in this style to the used properties list.
            // Do this here so only future rules are affect by properties used in this rule.
            for (var name in styleRules[i].usedProperties)
                usedProperties[name] = true;
        }

        if (priorityUsed) {
            // Walk the properties again and account for !important.
            var foundPriorityProperties = [];

            // Walk in reverse to match the order !important overrides.
            for (var i = (styleRules.length - 1); i >= 0; --i) {
                if (styleRules[i].computedStyle)
                    continue;

                var style = styleRules[i].style;
                var uniqueProperties = style.getUniqueProperties();
                for (var j = 0; j < uniqueProperties.length; ++j) {
                    var name = uniqueProperties[j];
                    if (style.getPropertyPriority(name).length) {
                        if (!(name in foundPriorityProperties))
                            styleRules[i].usedProperties[name] = true;
                        else
                            delete styleRules[i].usedProperties[name];
                        foundPriorityProperties[name] = true;
                    } else if (name in foundPriorityProperties)
                        delete styleRules[i].usedProperties[name];
                }
            }
        }

        if (refresh) {
            // Walk the style rules and update the sections with new overloaded and used properties.
            for (var i = 0; i < styleRules.length; ++i) {
                var styleRule = styleRules[i];
                var section = styleRule.section;
                section._usedProperties = (styleRule.usedProperties || usedProperties);
                section.update((section === editedSection) || styleRule.computedStyle);
            }
        } else {
            // Make a property section for each style rule.
            for (var i = 0; i < styleRules.length; ++i) {
                var styleRule = styleRules[i];
                var subtitle = styleRule.subtitle;
                delete styleRule.subtitle;

                var computedStyle = styleRule.computedStyle;
                delete styleRule.computedStyle;

                var ruleUsedProperties = styleRule.usedProperties;
                delete styleRule.usedProperties;

                var editable = styleRule.editable;
                delete styleRule.editable;

                // Default editable to true if it was omitted.
                if (typeof editable === "undefined")
                    editable = true;

                var section = new WebInspector.StylePropertiesSection(styleRule, subtitle, computedStyle, (ruleUsedProperties || usedProperties), editable);
                section.expanded = true;
                section.pane = this;

                body.appendChild(section.element);
                this.sections.push(section);
            }
        }
    }
}

WebInspector.StylesSidebarPane.prototype.__proto__ = WebInspector.SidebarPane.prototype;

WebInspector.StylePropertiesSection = function(styleRule, subtitle, computedStyle, usedProperties, editable)
{
    WebInspector.PropertiesSection.call(this, styleRule.selectorText);

    this.styleRule = styleRule;
    this.computedStyle = computedStyle;
    this.editable = (editable && !computedStyle);

    // Prevent editing the user agent rules.
    if (this.styleRule.parentStyleSheet && !this.styleRule.parentStyleSheet.ownerNode)
        this.editable = false;

    this._usedProperties = usedProperties;

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
        showInheritedLabel.appendChild(document.createTextNode(WebInspector.UIString("Show inherited properties")));
        this.subtitleElement.appendChild(showInheritedLabel);
    } else {
        if (!subtitle) {
            if (this.styleRule.parentStyleSheet && this.styleRule.parentStyleSheet.href) {
                var url = this.styleRule.parentStyleSheet.href;
                subtitle = WebInspector.linkifyURL(url, url.trimURL(WebInspector.mainResource.domain).escapeHTML());
                this.subtitleElement.addStyleClass("file");
            } else if (this.styleRule.parentStyleSheet && !this.styleRule.parentStyleSheet.ownerNode)
                subtitle = WebInspector.UIString("user agent stylesheet");
            else
                subtitle = WebInspector.UIString("inline stylesheet");
        }

        this.subtitle = subtitle;
    }
}

WebInspector.StylePropertiesSection.prototype = {
    get usedProperties()
    {
        return this._usedProperties || {};
    },

    set usedProperties(x)
    {
        this._usedProperties = x;
        this.update();
    },

    isPropertyInherited: function(property)
    {
        if (!this.computedStyle || !this._usedProperties)
            return false;
        // These properties should always show for Computed Style.
        var alwaysShowComputedProperties = { "display": true, "height": true, "width": true };
        return !(property in this.usedProperties) && !(property in alwaysShowComputedProperties);
    },

    isPropertyOverloaded: function(property, shorthand)
    {
        if (this.computedStyle || !this._usedProperties)
            return false;

        var used = (property in this.usedProperties);
        if (used || !shorthand)
            return !used;

        // Find out if any of the individual longhand properties of the shorthand
        // are used, if none are then the shorthand is overloaded too.
        var longhandProperties = this.styleRule.style.getLonghandProperties(property);
        for (var j = 0; j < longhandProperties.length; ++j) {
            var individualProperty = longhandProperties[j];
            if (individualProperty in this.usedProperties)
                return false;
        }

        return true;
    },

    update: function(full)
    {
        if (full || this.computedStyle) {
            this.propertiesTreeOutline.removeChildren();
            this.populated = false;
        } else {
            var child = this.propertiesTreeOutline.children[0];
            while (child) {
                child.overloaded = this.isPropertyOverloaded(child.name, child.shorthand);
                child = child.traverseNextTreeElement(false, null, true);
            }
        }
    },

    onpopulate: function()
    {
        var style = this.styleRule.style;
        if (!style.length)
            return;

        var foundShorthands = {};
        var uniqueProperties = style.getUniqueProperties();
        uniqueProperties.sort();

        for (var i = 0; i < uniqueProperties.length; ++i) {
            var name = uniqueProperties[i];
            var shorthand = style.getPropertyShorthand(name);

            if (shorthand && shorthand in foundShorthands)
                continue;

            if (shorthand) {
                foundShorthands[shorthand] = true;
                name = shorthand;
            }

            var isShorthand = (shorthand ? true : false);
            var inherited = this.isPropertyInherited(name);
            var overloaded = this.isPropertyOverloaded(name, isShorthand);

            var item = new WebInspector.StylePropertyTreeElement(style, name, isShorthand, inherited, overloaded);
            this.propertiesTreeOutline.appendChild(item);
        }
    }
}

WebInspector.StylePropertiesSection.prototype.__proto__ = WebInspector.PropertiesSection.prototype;

WebInspector.StylePropertyTreeElement = function(style, name, shorthand, inherited, overloaded)
{
    this.style = style;
    this.name = name;
    this.shorthand = shorthand;
    this._inherited = inherited;
    this._overloaded = overloaded;

    // Pass an empty title, the title gets made later in onattach.
    TreeElement.call(this, "", null, shorthand);
}

WebInspector.StylePropertyTreeElement.prototype = {
    get inherited()
    {
        return this._inherited;
    },

    set inherited(x)
    {
        if (x === this._inherited)
            return;
        this._inherited = x;
        this.updateState();
    },

    get overloaded()
    {
        return this._overloaded;
    },

    set overloaded(x)
    {
        if (x === this._overloaded)
            return;
        this._overloaded = x;
        this.updateState();
    },

    onattach: function()
    {
        this.updateTitle();
    },

    updateTitle: function()
    {
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

        var priority = (this.shorthand ? this.style.getShorthandPriority(this.name) : this.style.getPropertyPriority(this.name));
        var value = (this.shorthand ? this.style.getShorthandValue(this.name) : this.style.getPropertyValue(this.name));
        var htmlValue = value;

        if (priority && !priority.length)
            delete priority;
        if (priority)
            priority = "!" + priority;

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
        } else
            htmlValue = value = "";

        this.updateState();

        var nameElement = document.createElement("span");
        nameElement.className = "name";
        nameElement.textContent = this.name;

        var valueElement = document.createElement("span");
        valueElement.className = "value";
        valueElement.innerHTML = htmlValue;

        if (priority) {
            var priorityElement = document.createElement("span");
            priorityElement.className = "priority";
            priorityElement.textContent = priority;
        }

        this.listItemElement.removeChildren();

        this.listItemElement.appendChild(nameElement);
        this.listItemElement.appendChild(document.createTextNode(": "));
        this.listItemElement.appendChild(valueElement);

        if (priorityElement) {
            this.listItemElement.appendChild(document.createTextNode(" "));
            this.listItemElement.appendChild(priorityElement);
        }

        this.listItemElement.appendChild(document.createTextNode(";"));

        if (value) {
            // FIXME: this dosen't catch keyword based colors like black and white
            var colors = value.match(/((rgb|hsl)a?\([^)]+\))|(#[0-9a-fA-F]{6})|(#[0-9a-fA-F]{3})/g);
            if (colors) {
                var colorsLength = colors.length;
                for (var i = 0; i < colorsLength; ++i) {
                    var swatchElement = document.createElement("span");
                    swatchElement.className = "swatch";
                    swatchElement.style.setProperty("background-color", colors[i]);
                    this.listItemElement.appendChild(swatchElement);
                }
            }
        }

        this.tooltip = this.name + ": " + (valueNicknames[value] || value) + (priority ? " " + priority : "");
    },

    updateState: function()
    {
        if (!this.listItemElement)
            return;

        var value = (this.shorthand ? this.style.getShorthandValue(this.name) : this.style.getPropertyValue(this.name));
        if (this.style.isPropertyImplicit(this.name) || value === "initial")
            this.listItemElement.addStyleClass("implicit");
        else
            this.listItemElement.removeStyleClass("implicit");

        if (this.inherited)
            this.listItemElement.addStyleClass("inherited");
        else
            this.listItemElement.removeStyleClass("inherited");

        if (this.overloaded)
            this.listItemElement.addStyleClass("overloaded");
        else
            this.listItemElement.removeStyleClass("overloaded");
    },

    onpopulate: function()
    {
        // Only populate once and if this property is a shorthand.
        if (this.children.length || !this.shorthand)
            return;

        var longhandProperties = this.style.getLonghandProperties(this.name);
        for (var i = 0; i < longhandProperties.length; ++i) {
            var name = longhandProperties[i];

            if (this.treeOutline.section) {
                var inherited = this.treeOutline.section.isPropertyInherited(name);
                var overloaded = this.treeOutline.section.isPropertyOverloaded(name);
            }

            var item = new WebInspector.StylePropertyTreeElement(this.style, name, false, inherited, overloaded);
            this.appendChild(item);
        }
    },

    ondblclick: function(element, event)
    {
        this.startEditing(event.target);
    },

    startEditing: function(selectElement)
    {
        // FIXME: we don't allow editing of longhand properties under a shorthand right now.
        if (this.parent.shorthand)
            return;

        if (this.editing || (this.treeOutline.section && !this.treeOutline.section.editable))
            return;

        this.editing = true;
        this.previousTextContent = this.listItemElement.textContent;

        this.listItemElement.addStyleClass("focusable");
        this.listItemElement.addStyleClass("editing");
        this.wasExpanded = this.expanded;
        this.collapse();
        // Lie about out children to prevent toggling on click.
        this.hasChildren = false;

        if (!selectElement)
            selectElement = this.listItemElement;

        window.getSelection().setBaseAndExtent(selectElement, 0, selectElement, 1);

        var treeElement = this;
        this.listItemElement.blurred = function() { treeElement.commitEditing() };
        this.listItemElement.handleKeyEvent = function(event) {
            if (event.keyIdentifier === "Enter") {
                treeElement.commitEditing();
                event.preventDefault();
            } else if (event.keyCode === 27) { // Escape key
                treeElement.cancelEditing();
                event.preventDefault();
            }
        };

        this.previousFocusElement = WebInspector.currentFocusElement;
        WebInspector.currentFocusElement = this.listItemElement;
    },

    endEditing: function()
    {
        // Revert the changes done in startEditing().
        delete this.listItemElement.blurred;
        delete this.listItemElement.handleKeyEvent;

        WebInspector.currentFocusElement = this.previousFocusElement;
        delete this.previousFocusElement;

        delete this.previousTextContent;
        delete this.editing;

        this.listItemElement.removeStyleClass("focusable");
        this.listItemElement.removeStyleClass("editing");
        this.hasChildren = (this.children.length ? true : false);
        if (this.wasExpanded) {
            delete this.wasExpanded;
            this.expand();
        }
    },

    cancelEditing: function()
    {
        this.endEditing();
        this.updateTitle();
    },

    commitEditing: function()
    {
        var previousContent = this.previousTextContent;

        this.endEditing();

        var userInput = this.listItemElement.textContent;
        if (userInput === previousContent)
            return; // nothing changed, so do nothing else

        var userInputLength = userInput.trimWhitespace().length;

        // Create a new element to parse the user input CSS.
        var parseElement = document.createElement("span");
        parseElement.setAttribute("style", userInput);

        var userInputStyle = parseElement.style;
        if (userInputStyle.length || !userInputLength) {
            // The input was parsable or the user deleted everything, so remove the
            // original property from the real style declaration. If this represents
            // a shorthand remove all the longhand properties.
            if (this.shorthand) {
                var longhandProperties = this.style.getLonghandProperties(this.name);
                for (var i = 0; i < longhandProperties.length; ++i)
                    this.style.removeProperty(longhandProperties[i]);
            } else
                this.style.removeProperty(this.name);
        }

        if (!userInputLength) {
            // The user deleted the everything, so remove the tree element and update.
            if (this.treeOutline.section && this.treeOutline.section.pane)
                this.treeOutline.section.pane.update();
            this.parent.removeChild(this);
            return;
        }

        if (!userInputStyle.length) {
            // The user typed something, but it didn't parse. Just abort and restore
            // the original title for this property.
            this.updateTitle();
            return;
        }

        // Iterate of the properties on the test element's style declaration and
        // add them to the real style declaration. We take care to move shorthands.
        var foundShorthands = {};
        var uniqueProperties = userInputStyle.getUniqueProperties();
        for (var i = 0; i < uniqueProperties.length; ++i) {
            var name = uniqueProperties[i];
            var shorthand = userInputStyle.getPropertyShorthand(name);

            if (shorthand && shorthand in foundShorthands)
                continue;

            if (shorthand) {
                var value = userInputStyle.getShorthandValue(shorthand);
                var priority = userInputStyle.getShorthandPriority(shorthand);
                foundShorthands[shorthand] = true;
            } else {
                var value = userInputStyle.getPropertyValue(name);
                var priority = userInputStyle.getPropertyPriority(name);
            }

            // Set the property on the real style declaration.
            this.style.setProperty((shorthand || name), value, priority);
        }

        if (this.treeOutline.section && this.treeOutline.section.pane)
            this.treeOutline.section.pane.update(null, this.treeOutline.section);
        else if (this.treeOutline.section)
            this.treeOutline.section.update(true);
        else
            this.updateTitle(); // FIXME: this will not show new properties. But we don't hit his case yet.
    }
}

WebInspector.StylePropertyTreeElement.prototype.__proto__ = TreeElement.prototype;
