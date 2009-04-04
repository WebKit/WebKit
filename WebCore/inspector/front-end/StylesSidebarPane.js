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
    update: function(node, editedSection, forceUpdate)
    {
        var refresh = false;

        if (forceUpdate)
            delete this.node;

        if (!forceUpdate && (!node || node === this.node))
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

            if (node.style && (node.style.length || Object.hasProperties(node.style.__disabledProperties))) {
                var inlineStyle = { selectorText: WebInspector.UIString("Inline Style Attribute"), style: node.style };
                inlineStyle.subtitle = WebInspector.UIString("element’s “%s” attribute", "style");
                styleRules.push(inlineStyle);
            }

            var matchedStyleRules = node.ownerDocument.defaultView.getMatchedCSSRules(node, "", !Preferences.showUserAgentStyles);
            if (matchedStyleRules) {
                // Add rules in reverse order to match the cascade order.
                for (var i = (matchedStyleRules.length - 1); i >= 0; --i) {
                    var rule = matchedStyleRules[i];
                    styleRules.push({ style: rule.style, selectorText: rule.selectorText, parentStyleSheet: rule.parentStyleSheet });
                }
            }
        }

        function deleteDisabledProperty(style, name)
        {
            if (!style || !name)
                return;
            if (style.__disabledPropertyValues)
                delete style.__disabledPropertyValues[name];
            if (style.__disabledPropertyPriorities)
                delete style.__disabledPropertyPriorities[name];
            if (style.__disabledProperties)
                delete style.__disabledProperties[name];
        }

        var usedProperties = {};
        var disabledComputedProperties = {};
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

                // Delete any disabled properties, since the property does exist.
                // This prevents it from showing twice.
                deleteDisabledProperty(style, name);
                deleteDisabledProperty(style, style.getPropertyShorthand(name));
            }

            // Add all the properties found in this style to the used properties list.
            // Do this here so only future rules are affect by properties used in this rule.
            for (var name in styleRules[i].usedProperties)
                usedProperties[name] = true;

            // Remember all disabled properties so they show up in computed style.
            if (style.__disabledProperties)
                for (var name in style.__disabledProperties)
                    disabledComputedProperties[name] = true;
        }

        if (priorityUsed) {
            // Walk the properties again and account for !important.
            var foundPriorityProperties = [];

            // Walk in reverse to match the order !important overrides.
            for (var i = (styleRules.length - 1); i >= 0; --i) {
                if (styleRules[i].computedStyle)
                    continue;

                var style = styleRules[i].style;
                var uniqueProperties = getUniqueStyleProperties(style);
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
                if (styleRule.computedStyle)
                    section.disabledComputedProperties = disabledComputedProperties;
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
                if (computedStyle)
                    section.disabledComputedProperties = disabledComputedProperties;
                section.pane = this;

                if (Preferences.styleRulesExpandedState && section.identifier in Preferences.styleRulesExpandedState)
                    section.expanded = Preferences.styleRulesExpandedState[section.identifier];
                else if (computedStyle)
                    section.collapse(true);
                else
                    section.expand(true);

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

    // Prevent editing the user agent and user rules.
    var isUserAgent = this.styleRule.parentStyleSheet && !this.styleRule.parentStyleSheet.ownerNode && !this.styleRule.parentStyleSheet.href;
    var isUser = this.styleRule.parentStyleSheet && this.styleRule.parentStyleSheet.ownerNode && this.styleRule.parentStyleSheet.ownerNode.nodeName == '#document';
    if (isUserAgent || isUser)
        this.editable = false;

    this._usedProperties = usedProperties;

    if (computedStyle) {
        this.element.addStyleClass("computed-style");

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
        showInheritedLabel.appendChild(document.createTextNode(WebInspector.UIString("Show inherited")));
        this.subtitleElement.appendChild(showInheritedLabel);
    } else {
        if (!subtitle) {
            if (this.styleRule.parentStyleSheet && this.styleRule.parentStyleSheet.href) {
                var url = this.styleRule.parentStyleSheet.href;
                subtitle = WebInspector.linkifyURL(url, WebInspector.displayNameForURL(url));
                this.subtitleElement.addStyleClass("file");
            } else if (isUserAgent)
                subtitle = WebInspector.UIString("user agent stylesheet");
            else if (isUser)
                subtitle = WebInspector.UIString("user stylesheet");
            else
                subtitle = WebInspector.UIString("inline stylesheet");
        }

        this.subtitle = subtitle;
    }

    this.identifier = styleRule.selectorText;
    if (this.subtitle)
        this.identifier += ":" + this.subtitleElement.textContent;
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

    expand: function(dontRememberState)
    {
        WebInspector.PropertiesSection.prototype.expand.call(this);
        if (dontRememberState)
            return;

        if (!Preferences.styleRulesExpandedState)
            Preferences.styleRulesExpandedState = {};
        Preferences.styleRulesExpandedState[this.identifier] = true;
    },

    collapse: function(dontRememberState)
    {
        WebInspector.PropertiesSection.prototype.collapse.call(this);
        if (dontRememberState)
            return;

        if (!Preferences.styleRulesExpandedState)
            Preferences.styleRulesExpandedState = {};
        Preferences.styleRulesExpandedState[this.identifier] = false;
    },

    isPropertyInherited: function(property)
    {
        if (!this.computedStyle || !this._usedProperties)
            return false;
        // These properties should always show for Computed Style.
        var alwaysShowComputedProperties = { "display": true, "height": true, "width": true };
        return !(property in this.usedProperties) && !(property in alwaysShowComputedProperties) && !(property in this.disabledComputedProperties);
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
        var longhandProperties = getLonghandProperties(this.styleRule.style, property);
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

        var foundShorthands = {};
        var uniqueProperties = getUniqueStyleProperties(style);
        var disabledProperties = style.__disabledPropertyValues || {};

        for (var name in disabledProperties)
            uniqueProperties.push(name);

        uniqueProperties.sort();

        for (var i = 0; i < uniqueProperties.length; ++i) {
            var name = uniqueProperties[i];
            var disabled = name in disabledProperties;
            if (!disabled && this.disabledComputedProperties && !(name in this.usedProperties) && name in this.disabledComputedProperties)
                disabled = true;

            var shorthand = !disabled ? style.getPropertyShorthand(name) : null;

            if (shorthand && shorthand in foundShorthands)
                continue;

            if (shorthand) {
                foundShorthands[shorthand] = true;
                name = shorthand;
            }

            var isShorthand = (shorthand ? true : false);
            var inherited = this.isPropertyInherited(name);
            var overloaded = this.isPropertyOverloaded(name, isShorthand);

            var item = new WebInspector.StylePropertyTreeElement(style, name, isShorthand, inherited, overloaded, disabled);
            this.propertiesTreeOutline.appendChild(item);
        }
    }
}

WebInspector.StylePropertiesSection.prototype.__proto__ = WebInspector.PropertiesSection.prototype;

WebInspector.StylePropertyTreeElement = function(style, name, shorthand, inherited, overloaded, disabled)
{
    this.style = style;
    this.name = name;
    this.shorthand = shorthand;
    this._inherited = inherited;
    this._overloaded = overloaded;
    this._disabled = disabled;

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

    get disabled()
    {
        return this._disabled;
    },

    set disabled(x)
    {
        if (x === this._disabled)
            return;
        this._disabled = x;
        this.updateState();
    },

    get priority()
    {
        if (this.disabled && this.style.__disabledPropertyPriorities && this.name in this.style.__disabledPropertyPriorities)
            return this.style.__disabledPropertyPriorities[this.name];
        return (this.shorthand ? getShorthandPriority(this.style, this.name) : this.style.getPropertyPriority(this.name));
    },

    get value()
    {
        if (this.disabled && this.style.__disabledPropertyValues && this.name in this.style.__disabledPropertyValues)
            return this.style.__disabledPropertyValues[this.name];
        return (this.shorthand ? getShorthandValue(this.style, this.name) : this.style.getPropertyValue(this.name));
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

        var priority = this.priority;
        var value = this.value;
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

        var enabledCheckboxElement = document.createElement("input");
        enabledCheckboxElement.className = "enabled-button";
        enabledCheckboxElement.type = "checkbox";
        enabledCheckboxElement.checked = !this.disabled;
        enabledCheckboxElement.addEventListener("change", this.toggleEnabled.bind(this), false);

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

        // Append the checkbox for root elements of an editable section.
        if (this.treeOutline.section && this.treeOutline.section.editable && this.parent.root)
            this.listItemElement.appendChild(enabledCheckboxElement);
        this.listItemElement.appendChild(nameElement);
        this.listItemElement.appendChild(document.createTextNode(": "));
        this.listItemElement.appendChild(valueElement);

        if (priorityElement) {
            this.listItemElement.appendChild(document.createTextNode(" "));
            this.listItemElement.appendChild(priorityElement);
        }

        this.listItemElement.appendChild(document.createTextNode(";"));

        if (value) {
            // FIXME: this only covers W3C and CSS 16 valid color names
            var colors = value.match(/((rgb|hsl)a?\([^)]+\))|(#[0-9a-fA-F]{6})|(#[0-9a-fA-F]{3})|aqua|black|blue|fuchsia|gray|green|lime|maroon|navy|olive|purple|red|silver|teal|white|yellow/g);
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

    updateAll: function(updateAllRules)
    {
        if (updateAllRules && this.treeOutline.section && this.treeOutline.section.pane)
            this.treeOutline.section.pane.update(null, this.treeOutline.section);
        else if (this.treeOutline.section)
            this.treeOutline.section.update(true);
        else
            this.updateTitle(); // FIXME: this will not show new properties. But we don't hit his case yet.
    },

    toggleEnabled: function(event)
    {
        var disabled = !event.target.checked;

        if (disabled) {
            if (!this.style.__disabledPropertyValues || !this.style.__disabledPropertyPriorities) {
                var inspectedWindow = InspectorController.inspectedWindow();
                this.style.__disabledProperties = new inspectedWindow.Object;
                this.style.__disabledPropertyValues = new inspectedWindow.Object;
                this.style.__disabledPropertyPriorities = new inspectedWindow.Object;
            }

            this.style.__disabledPropertyValues[this.name] = this.value;
            this.style.__disabledPropertyPriorities[this.name] = this.priority;

            if (this.shorthand) {
                var longhandProperties = getLonghandProperties(this.style, this.name);
                for (var i = 0; i < longhandProperties.length; ++i) {
                    this.style.__disabledProperties[longhandProperties[i]] = true;
                    this.style.removeProperty(longhandProperties[i]);
                }
            } else {
                this.style.__disabledProperties[this.name] = true;
                this.style.removeProperty(this.name);
            }
        } else {
            this.style.setProperty(this.name, this.value, this.priority);
            delete this.style.__disabledProperties[this.name];
            delete this.style.__disabledPropertyValues[this.name];
            delete this.style.__disabledPropertyPriorities[this.name];
        }

        // Set the disabled property here, since the code above replies on it not changing
        // until after the value and priority are retrieved.
        this.disabled = disabled;

        if (this.treeOutline.section && this.treeOutline.section.pane)
            this.treeOutline.section.pane.dispatchEventToListeners("style property toggled");

        this.updateAll(true);
    },

    updateState: function()
    {
        if (!this.listItemElement)
            return;

        if (this.style.isPropertyImplicit(this.name) || this.value === "initial")
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

        if (this.disabled)
            this.listItemElement.addStyleClass("disabled");
        else
            this.listItemElement.removeStyleClass("disabled");
    },

    onpopulate: function()
    {
        // Only populate once and if this property is a shorthand.
        if (this.children.length || !this.shorthand)
            return;

        var longhandProperties = getLonghandProperties(this.style, this.name);
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

        if (WebInspector.isBeingEdited(this.listItemElement) || (this.treeOutline.section && !this.treeOutline.section.editable))
            return;

        var context = { expanded: this.expanded, hasChildren: this.hasChildren };

        // Lie about our children to prevent expanding on double click and to collapse shorthands.
        this.hasChildren = false;

        if (!selectElement)
            selectElement = this.listItemElement;

        this.listItemElement.handleKeyEvent = this.editingKeyDown.bind(this);

        WebInspector.startEditing(this.listItemElement, this.editingCommitted.bind(this), this.editingCancelled.bind(this), context);
        window.getSelection().setBaseAndExtent(selectElement, 0, selectElement, 1);
    },

    editingKeyDown: function(event)
    {
        var arrowKeyPressed = (event.keyIdentifier === "Up" || event.keyIdentifier === "Down");
        var pageKeyPressed = (event.keyIdentifier === "PageUp" || event.keyIdentifier === "PageDown");
        if (!arrowKeyPressed && !pageKeyPressed)
            return;

        var selection = window.getSelection();
        if (!selection.rangeCount)
            return;

        var selectionRange = selection.getRangeAt(0);
        if (selectionRange.commonAncestorContainer !== this.listItemElement && !selectionRange.commonAncestorContainer.isDescendant(this.listItemElement))
            return;

        const styleValueDelimeters = " \t\n\"':;,/()";
        var wordRange = selectionRange.startContainer.rangeOfWord(selectionRange.startOffset, styleValueDelimeters, this.listItemElement);
        var wordString = wordRange.toString();
        var replacementString = wordString;

        var matches = /(.*?)(-?\d+(?:\.\d+)?)(.*)/.exec(wordString);
        if (matches && matches.length) {
            var prefix = matches[1];
            var number = parseFloat(matches[2]);
            var suffix = matches[3];

            // If the number is near zero or the number is one and the direction will take it near zero.
            var numberNearZero = (number < 1 && number > -1);
            if (number === 1 && event.keyIdentifier === "Down")
                numberNearZero = true;
            else if (number === -1 && event.keyIdentifier === "Up")
                numberNearZero = true;

            if (numberNearZero && event.altKey && arrowKeyPressed) {
                if (event.keyIdentifier === "Down")
                    number = Math.ceil(number - 1);
                else
                    number = Math.floor(number + 1);
            } else {
                // Jump by 10 when shift is down or jump by 0.1 when near zero or Alt/Option is down.
                // Also jump by 10 for page up and down, or by 100 if shift is held with a page key.
                var changeAmount = 1;
                if (event.shiftKey && pageKeyPressed)
                    changeAmount = 100;
                else if (event.shiftKey || pageKeyPressed)
                    changeAmount = 10;
                else if (event.altKey || numberNearZero)
                    changeAmount = 0.1;

                if (event.keyIdentifier === "Down" || event.keyIdentifier === "PageDown")
                    changeAmount *= -1;

                // Make the new number and constrain it to a precision of 6, this matches numbers the engine returns.
                // Use the Number constructor to forget the fixed precision, so 1.100000 will print as 1.1.
                number = Number((number + changeAmount).toFixed(6));
            }

            replacementString = prefix + number + suffix;
        } else {
            // FIXME: this should cycle through known keywords for the current property name.
            return;
        }

        var replacementTextNode = document.createTextNode(replacementString);

        wordRange.deleteContents();
        wordRange.insertNode(replacementTextNode);

        var finalSelectionRange = document.createRange();
        finalSelectionRange.setStart(replacementTextNode, 0);
        finalSelectionRange.setEnd(replacementTextNode, replacementString.length);

        selection.removeAllRanges();
        selection.addRange(finalSelectionRange);

        event.preventDefault();
        event.handled = true;

        if (!this.originalCSSText) {
            // Remember the rule's original CSS text, so it can be restored
            // if the editing is canceled and before each apply.
            this.originalCSSText = getStyleTextWithShorthands(this.style);
        } else {
            // Restore the original CSS text before applying user changes. This is needed to prevent
            // new properties from sticking around if the user adds one, then removes it.
            this.style.cssText = this.originalCSSText;
        }

        this.applyStyleText(this.listItemElement.textContent);
    },

    editingEnded: function(context)
    {
        this.hasChildren = context.hasChildren;
        if (context.expanded)
            this.expand();
        delete this.listItemElement.handleKeyEvent;
        delete this.originalCSSText;
    },

    editingCancelled: function(element, context)
    {
        if (this.originalCSSText) {
            this.style.cssText = this.originalCSSText;

            if (this.treeOutline.section && this.treeOutline.section.pane)
                this.treeOutline.section.pane.dispatchEventToListeners("style edited");

            this.updateAll();
        } else
            this.updateTitle();

        this.editingEnded(context);
    },

    editingCommitted: function(element, userInput, previousContent, context)
    {
        this.editingEnded(context);

        if (userInput === previousContent)
            return; // nothing changed, so do nothing else

        this.applyStyleText(userInput, true);
    },

    applyStyleText: function(styleText, updateInterface)
    {
        var styleTextLength = styleText.trimWhitespace().length;

        // Create a new element to parse the user input CSS.
        var parseElement = document.createElement("span");
        parseElement.setAttribute("style", styleText);

        var tempStyle = parseElement.style;
        if (tempStyle.length || !styleTextLength) {
            // The input was parsable or the user deleted everything, so remove the
            // original property from the real style declaration. If this represents
            // a shorthand remove all the longhand properties.
            if (this.shorthand) {
                var longhandProperties = getLonghandProperties(this.style, this.name);
                for (var i = 0; i < longhandProperties.length; ++i)
                    this.style.removeProperty(longhandProperties[i]);
            } else
                this.style.removeProperty(this.name);
        }

        if (!styleTextLength) {
            if (updateInterface) {
                // The user deleted the everything, so remove the tree element and update.
                if (this.treeOutline.section && this.treeOutline.section.pane)
                    this.treeOutline.section.pane.update();
                this.parent.removeChild(this);
            }
            return;
        }

        if (!tempStyle.length) {
            // The user typed something, but it didn't parse. Just abort and restore
            // the original title for this property.
            if (updateInterface)
                this.updateTitle();
            return;
        }

        // Iterate of the properties on the test element's style declaration and
        // add them to the real style declaration. We take care to move shorthands.
        var foundShorthands = {};
        var uniqueProperties = getUniqueStyleProperties(tempStyle);
        for (var i = 0; i < uniqueProperties.length; ++i) {
            var name = uniqueProperties[i];
            var shorthand = tempStyle.getPropertyShorthand(name);

            if (shorthand && shorthand in foundShorthands)
                continue;

            if (shorthand) {
                var value = getShorthandValue(tempStyle, shorthand);
                var priority = getShorthandPriority(tempStyle, shorthand);
                foundShorthands[shorthand] = true;
            } else {
                var value = tempStyle.getPropertyValue(name);
                var priority = tempStyle.getPropertyPriority(name);
            }

            // Set the property on the real style declaration.
            this.style.setProperty((shorthand || name), value, priority);
        }

        if (this.treeOutline.section && this.treeOutline.section.pane)
            this.treeOutline.section.pane.dispatchEventToListeners("style edited");

        if (updateInterface)
            this.updateAll(true);
    }
}

WebInspector.StylePropertyTreeElement.prototype.__proto__ = TreeElement.prototype;
