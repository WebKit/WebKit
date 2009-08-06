/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2009 Joseph Pecoraro
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

        var self = this;
        var callback = function(styles) {
            if (!styles)
                return;
            var nodeWrapper = WebInspector.wrapNodeWithStyles(node, styles);
            self._update(refresh, body, nodeWrapper, editedSection, forceUpdate);
        };
        InspectorController.getStyles(node, !Preferences.showUserAgentStyles, callback);
    },

    _update: function(refresh, body, node, editedSection, forceUpdate)
    {
        if (!refresh) {
            body.removeChildren();
            this.sections = [];
        }

        var styleRules = [];

        if (refresh) {
            for (var i = 0; i < this.sections.length; ++i) {
                var section = this.sections[i];
                if (section._blank)
                    continue;
                if (section.computedStyle)
                    section.styleRule.style = node.ownerDocument.defaultView.getComputedStyle(node);
                var styleRule = { section: section, style: section.styleRule.style, computedStyle: section.computedStyle, rule: section.rule };
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

            // Always Show element's Style Attributes
            if (node.nodeType === Node.ELEMENT_NODE) {
                var inlineStyle = { selectorText: WebInspector.UIString("Style Attribute"), style: node.style, isAttribute: true };
                inlineStyle.subtitle = WebInspector.UIString("element’s “%s” attribute", "style");
                styleRules.push(inlineStyle);
            }

            var matchedStyleRules = node.ownerDocument.defaultView.getMatchedCSSRules(node, "", !Preferences.showUserAgentStyles);
            if (matchedStyleRules) {
                // Add rules in reverse order to match the cascade order.
                for (var i = (matchedStyleRules.length - 1); i >= 0; --i) {
                    var rule = matchedStyleRules[i];
                    styleRules.push({ style: rule.style, selectorText: rule.selectorText, parentStyleSheet: rule.parentStyleSheet, rule: rule });
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
            if (styleRule.section && styleRule.section.noAffect)
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
                var uniqueProperties = style.uniqueStyleProperties;
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

                var isAttribute = styleRule.isAttribute;
                delete styleRule.isAttribute;

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
                else if (isAttribute && styleRule.style.length === 0)
                    section.collapse(true);
                else
                    section.expand(true);

                body.appendChild(section.element);
                this.sections.push(section);
            }

            this.addBlankSection();
        }
    },

    addBlankSection: function()
    {
        var blankSection = new WebInspector.BlankStylePropertiesSection();
        blankSection.pane = this;

        this.bodyElement.insertBefore(blankSection.element, this.bodyElement.firstChild.nextSibling.nextSibling); // 0 is computed, 1 is element.style
        var computed = this.sections.shift();
        var elementStyle = this.sections.shift();
        this.sections.unshift(blankSection);
        this.sections.unshift(elementStyle);
        this.sections.unshift(computed);
    },

    appropriateSelectorForNode: function()
    {
        var node = this.node;
        if (!node)
            return;

        if (node.id)
            return "#" + node.id;

        if (node.className)
            return "." + node.className.replace(/\s+/, ".");

        var nodeName = node.nodeName.toLowerCase();
        if (nodeName === "input" && node.type)
            return nodeName + "[type=\"" + node.type + "\"]";

        return nodeName;
    }
}

WebInspector.StylesSidebarPane.prototype.__proto__ = WebInspector.SidebarPane.prototype;

WebInspector.StylePropertiesSection = function(styleRule, subtitle, computedStyle, usedProperties, editable)
{
    WebInspector.PropertiesSection.call(this, styleRule.selectorText);
    this.titleElement.addEventListener("click", function(e) { e.stopPropagation(); }, false);
    this.titleElement.addEventListener("dblclick", this._dblclickSelector.bind(this), false);
    this.element.addEventListener("dblclick", this._dblclickEmptySpace.bind(this), false);

    this.styleRule = styleRule;
    this.rule = this.styleRule.rule;
    this.computedStyle = computedStyle;
    this.editable = (editable && !computedStyle);

    // Prevent editing the user agent and user rules.
    var isUserAgent = this.styleRule.isUserAgent;
    var isUser = this.styleRule.isUser;

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
            else if (this.styleRule.parentStyleSheet === WebInspector.panels.elements.stylesheet)
                subtitle = WebInspector.UIString("via inspector");
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
        if (this._blank)
            return;

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
        if (!this.computedStyle || !this._usedProperties || this.noAffect)
            return false;
        // These properties should always show for Computed Style.
        var alwaysShowComputedProperties = { "display": true, "height": true, "width": true };
        return !(property in this.usedProperties) && !(property in alwaysShowComputedProperties) && !(property in this.disabledComputedProperties);
    },

    isPropertyOverloaded: function(property, shorthand)
    {
        if (this.computedStyle || !this._usedProperties || this.noAffect)
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

    isInspectorStylesheet: function()
    {
        return (this.styleRule.parentStyleSheet === WebInspector.panels.elements.stylesheet);
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

        if (this._afterUpdate) {
            this._afterUpdate(this);
            delete this._afterUpdate;
        }
    },

    onpopulate: function()
    {
        var style = this.styleRule.style;

        var foundShorthands = {};
        var uniqueProperties = style.uniqueStyleProperties;
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

            var item = new WebInspector.StylePropertyTreeElement(this.styleRule, style, name, isShorthand, inherited, overloaded, disabled);
            this.propertiesTreeOutline.appendChild(item);
        }
    },

    findTreeElementWithName: function(name)
    {
        var treeElement = this.propertiesTreeOutline.children[0];
        while (treeElement) {
            if (treeElement.name === name)
                return treeElement;
            treeElement = treeElement.traverseNextTreeElement(true, null, true);
        }
        return null;
    },

    addNewBlankProperty: function()
    {
        var item = new WebInspector.StylePropertyTreeElement(this.styleRule, this.styleRule.style, "", false, false, false, false);
        this.propertiesTreeOutline.appendChild(item);
        item.listItemElement.textContent = "";
        item._newProperty = true;
        return item;
    },

    _dblclickEmptySpace: function(event)
    {
        this.expand();
        this.addNewBlankProperty().startEditing();
    },

    _dblclickSelector: function(event)
    {
        if (!this.editable)
            return;

        if (!this.rule && this.propertiesTreeOutline.children.length === 0) {
            this.expand();
            this.addNewBlankProperty().startEditing();
            return;
        }

        if (!this.rule)
            return;

        this.startEditingSelector();
        event.stopPropagation();
    },

    startEditingSelector: function()
    {
        var element = this.titleElement;
        if (WebInspector.isBeingEdited(element))
            return;

        var context = this.styleRule.selectorText;
        WebInspector.startEditing(this.titleElement, this.editingSelectorCommitted.bind(this), this.editingSelectorCancelled.bind(this), context);
        window.getSelection().setBaseAndExtent(element, 0, element, 1);
    },

    editingSelectorCommitted: function(element, newContent, oldContent, context, moveDirection)
    {
        function moveToNextIfNeeded() {
            if (!moveDirection || moveDirection !== "forward")
                return;

            this.expand();
            if (this.propertiesTreeOutline.children.length === 0)
                this.addNewBlankProperty().startEditing();
            else {
                var item = this.propertiesTreeOutline.children[0]
                item.startEditing(item.valueElement);
            }
        }

        if (newContent === oldContent)
            return moveToNextIfNeeded.call(this);

        var self = this;
        var callback = function(result) {
            if (!result) {
                // Invalid Syntax for a Selector
                self.editingSelectorCancelled(element, context);
                moveToNextIfNeeded.call(self);
                return;
            }

            var newRulePayload = result[0];
            var doesAffectSelectedNode = result[1];
            if (!doesAffectSelectedNode) {
                self.noAffect = true;
                self.element.addStyleClass("no-affect");
            } else {
                delete self.noAffect;
                self.element.removeStyleClass("no-affect");
            }

            var newRule = WebInspector.CSSStyleDeclaration.parseRule(newRulePayload);
            self.rule = newRule;
            self.styleRule = { section: self, style: newRule.style, selectorText: newRule.selectorText, parentStyleSheet: newRule.parentStyleSheet, rule: newRule };
            var oldIdentifier = this.identifier;
            self.identifier = newRule.selectorText + ":" + self.subtitleElement.textContent;
            self.pane.update(null, true);
            WebInspector.panels.elements.renameSelector(oldIdentifier, this.identifier, oldContent, newContent);
            moveToNextIfNeeded.call(self);
        };

        InspectorController.applyStyleRuleText(this.rule._id, newContent, this.pane.node, callback);
    },

    editingSelectorCancelled: function(element, context)
    {
        element.textContent = context;
    },

    _doesSelectorAffectSelectedNode: function(selector)
    {
        var selectedNode = this.pane.node;
        var nodes = selectedNode.ownerDocument.querySelectorAll(selector);
        for (var i = 0; i < nodes.length; ++i) {
            if (nodes[i] === selectedNode)
                return true;
        }

        return false;
    }
}

WebInspector.StylePropertiesSection.prototype.__proto__ = WebInspector.PropertiesSection.prototype;

WebInspector.BlankStylePropertiesSection = function()
{
    WebInspector.PropertiesSection.call(this, WebInspector.UIString("Double-Click to Add"), null);

    this._blank = true;
    this._dblclickListener = this._dblclick.bind(this);
    this.element.addStyleClass("blank-section");
    this.titleElement.addStyleClass("blank-title");
    this.titleElement.addEventListener("click", function(e) { e.stopPropagation(); }, false);
    this.titleElement.addEventListener("dblclick", this._dblclickListener, false);
}

WebInspector.BlankStylePropertiesSection.prototype = {
    _dblclick: function(event)
    {
        this.startEditing();
    },

    startEditing: function()
    {
        var element = this.titleElement;
        if (WebInspector.isBeingEdited(element))
            return;

        this.titleElement.textContent = this.pane.appropriateSelectorForNode();
        this.titleElement.removeStyleClass("blank-title");
        WebInspector.startEditing(this.titleElement, this.editingCommitted.bind(this), this.editingCancelled.bind(this), "");
        window.getSelection().setBaseAndExtent(element, 0, element, 1);
    },

    editingCancelled: function()
    {
        this.titleElement.textContent = WebInspector.UIString("Double-Click to Add");
        this.titleElement.addStyleClass("blank-title");
    },

    editingCommitted: function(element, newContent, oldContent, context)
    {
        var self = this;
        var callback = function(styleRule) {
            if (!styleRule) {
                // Invalid Syntax for a Selector
                self.editingCancelled();
                return;
            }
            self.makeNormal(WebInspector.CSSStyleDeclaration.parseRule(styleRule));

            if (!self._doesSelectorAffectSelectedNode(newContent)) {
                self.noAffect = true;
                self.element.addStyleClass("no-affect");
            }

            self.subtitleElement.textContent = WebInspector.UIString("via inspector");
            self.expand();

            self.pane.addBlankSection();
            self.addNewBlankProperty().startEditing();
        };
        InspectorController.addStyleSelector(newContent, callback);
    },

    makeNormal: function(styleRule)
    {
        this.titleElement.removeEventListener("dblclick", this._dblclickListener, false);
        this.titleElement.addEventListener("dblclick", this._dblclickSelector.bind(this), false);
        this.element.addEventListener("dblclick", this._dblclickEmptySpace.bind(this), false);
        this.element.removeStyleClass("blank-section");
        delete this._blank;
        delete this._dblclick;
        delete this.startEditing;
        delete this.editingCancelled;
        delete this.editingCommitted;
        delete this._dblclickListener;
        delete this.makeNormal;
        this.styleRule = styleRule;
        this.rule = styleRule.rule;
        this.computedStyle = false;
        this.editable = true;
        this.identifier = styleRule.selectorText + ":inspector";
        // leftovers are: this.noAffect if applicable
    }
}

WebInspector.BlankStylePropertiesSection.prototype.__proto__ = WebInspector.StylePropertiesSection.prototype;

WebInspector.StylePropertyTreeElement = function(styleRule, style, name, shorthand, inherited, overloaded, disabled)
{
    this._styleRule = styleRule;
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
        return (this.shorthand ? this.style.getShorthandPriority(this.name) : this.style.getPropertyPriority(this.name));
    },

    get value()
    {
        if (this.disabled && this.style.__disabledPropertyValues && this.name in this.style.__disabledPropertyValues)
            return this.style.__disabledPropertyValues[this.name];
        return (this.shorthand ? this.style.getShorthandValue(this.name) : this.style.getPropertyValue(this.name));
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
        this.nameElement = nameElement;

        var valueElement = document.createElement("span");
        valueElement.className = "value";
        valueElement.innerHTML = htmlValue;
        this.valueElement = valueElement;

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
            var colors = value.match(/((rgb|hsl)a?\([^)]+\))|(#[0-9a-fA-F]{6})|(#[0-9a-fA-F]{3})|aqua|black|blue|fuchsia|gray|green|lime|maroon|navy|olive|purple|red|silver|teal|white|yellow|transparent/g);
            var swatch;
            if (colors) {
                var colorsLength = colors.length;
                for (var i = 0; i < colorsLength; ++i) {
                    var swatchElement = document.createElement("span");
                    swatchElement.className = "swatch";
                    swatchElement.style.setProperty("background-color", colors[i]);
                    this.listItemElement.appendChild(swatchElement);
                    swatch = swatchElement;
                }
            }

            // Rotate through Color Representations by Clicking on the Swatch
            // Simple: rgb -> hsl -> nickname? -> shorthex? -> hex -> ...
            // Advanced: rgba -> hsla -> nickname? -> ...            
            if (colors && colors.length === 1) {
                var color = new WebInspector.Color(htmlValue);
                swatch.addEventListener("click", changeColorDisplay, false);
                swatch.addEventListener("dblclick", function(event) {
                    event.stopPropagation();
                }, false);

                var mode = color.mode;
                var valueElement = this.valueElement;
                function changeColorDisplay(event) {

                    function changeTo(newMode, content) {
                        mode = newMode;
                        valueElement.textContent = content;
                    }

                    switch (mode) {
                        case "rgb":
                            changeTo("hsl", color.toHsl());
                            break;

                        case "shorthex":
                            changeTo("hex", color.toHex());
                            break;

                        case "hex":
                            changeTo("rgb", color.toRgb());
                            break;

                        case "nickname":
                            if (color.simple) {
                                if (color.hasShortHex())
                                    changeTo("shorthex", color.toShortHex());
                                else
                                    changeTo("hex", color.toHex());
                            } else
                                changeTo("rgba", color.toRgba());
                            break;

                        case "hsl":
                            if (color.nickname)
                                changeTo("nickname", color.toNickname());
                            else if (color.hasShortHex())
                                changeTo("shorthex", color.toShortHex());
                            else
                                changeTo("hex", color.toHex());
                            break;

                        case "rgba":
                            changeTo("hsla", color.toHsla());
                            break;

                        case "hsla":
                            if (color.nickname)
                                changeTo("nickname", color.toNickname());
                            else
                                changeTo("rgba", color.toRgba());
                            break;
                    }
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

        var self = this;
        var callback = function(newPayload) {
            if (!newPayload)
                return;

            self.style = WebInspector.CSSStyleDeclaration.parseStyle(newPayload);
            self._styleRule.style = self.style;

            // Set the disabled property here, since the code above replies on it not changing
            // until after the value and priority are retrieved.
            self.disabled = disabled;

            if (self.treeOutline.section && self.treeOutline.section.pane)
                self.treeOutline.section.pane.dispatchEventToListeners("style property toggled");

            self.updateAll(true);
        };
        InspectorController.toggleStyleEnabled(this.style._id, this.name, disabled, callback);
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

        var longhandProperties = this.style.getLonghandProperties(this.name);
        for (var i = 0; i < longhandProperties.length; ++i) {
            var name = longhandProperties[i];

            if (this.treeOutline.section) {
                var inherited = this.treeOutline.section.isPropertyInherited(name);
                var overloaded = this.treeOutline.section.isPropertyOverloaded(name);
            }

            var item = new WebInspector.StylePropertyTreeElement(this._styleRule, this.style, name, false, inherited, overloaded);
            this.appendChild(item);
        }
    },

    ondblclick: function(element, event)
    {
        this.startEditing(event.target);
        event.stopPropagation();
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
            this.originalCSSText = this.style.styleTextWithShorthands();
        } else {
            // Restore the original CSS text before applying user changes. This is needed to prevent
            // new properties from sticking around if the user adds one, then removes it.
            InspectorController.setStyleText(this.style, this.originalCSSText);
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
        if (this._newProperty)
            this.treeOutline.removeChild(this);
        else if (this.originalCSSText) {
            InspectorController.setStyleText(this.style, this.originalCSSText);

            if (this.treeOutline.section && this.treeOutline.section.pane)
                this.treeOutline.section.pane.dispatchEventToListeners("style edited");

            this.updateAll();
        } else
            this.updateTitle();

        this.editingEnded(context);
    },

    editingCommitted: function(element, userInput, previousContent, context, moveDirection)
    {
        this.editingEnded(context);

        // Determine where to move to before making changes
        var newProperty, moveToPropertyName, moveToSelector;
        var moveTo = (moveDirection === "forward" ? this.nextSibling : this.previousSibling);
        if (moveTo)
            moveToPropertyName = moveTo.name;
        else if (moveDirection === "forward")
            newProperty = true;
        else if (moveDirection === "backward" && this.treeOutline.section.rule)
            moveToSelector = true;

        // Make the Changes and trigger the moveToNextCallback after updating
        var blankInput = /^\s*$/.test(userInput);
        if (userInput !== previousContent || (this._newProperty && blankInput)) { // only if something changed, or adding a new style and it was blank
            this.treeOutline.section._afterUpdate = moveToNextCallback.bind(this, this._newProperty, !blankInput);
            this.applyStyleText(userInput, true);
        } else
            moveToNextCallback(this._newProperty, false, this.treeOutline.section, false);

        // The Callback to start editing the next property
        function moveToNextCallback(alreadyNew, valueChanged, section) {
            if (!moveDirection)
                return;

            // User just tabbed through without changes
            if (moveTo && moveTo.parent) {
                moveTo.startEditing(moveTo.valueElement);
                return;
            }

            // User has made a change then tabbed, wiping all the original treeElements,
            // recalculate the new treeElement for the same property we were going to edit next
            if (moveTo && !moveTo.parent) {
                var treeElement = section.findTreeElementWithName(moveToPropertyName);
                if (treeElement)
                    treeElement.startEditing(treeElement.valueElement);
                return;
            }

            // Create a new attribute in this section
            if (newProperty) {
                if (alreadyNew && !valueChanged)
                    return;

                var item = section.addNewBlankProperty();
                item.startEditing();
                return;
            }

            if (moveToSelector)
                section.startEditingSelector();
        }
    },

    applyStyleText: function(styleText, updateInterface)
    {
        var section = this.treeOutline.section;
        var elementsPanel = WebInspector.panels.elements;
        var styleTextLength = styleText.trimWhitespace().length;
        if (!styleTextLength) {
            if (updateInterface) {
                // The user deleted everything, so remove the tree element and update.
                if (!this._newProperty)
                    delete section._afterUpdate;
                if (section && section.pane)
                    section.pane.update();
                this.parent.removeChild(this);
                elementsPanel.removeStyleChange(section.identifier, this.style, this.name);
            }
            return;
        }

        var self = this;
        var callback = function(result) {
            if (!result) {
                // The user typed something, but it didn't parse. Just abort and restore
                // the original title for this property.  If this was a new attribute and
                // we couldn't parse, then just remove it.
                if (self._newProperty) {
                    self.parent.removeChild(self);
                    return;
                }
                if (updateInterface)
                    self.updateTitle();
                return;
            }

            var newPayload = result[0];
            var changedProperties = result[1];
            elementsPanel.removeStyleChange(section.identifier, self.style, self.name);

            self.style = WebInspector.CSSStyleDeclaration.parseStyle(newPayload);
            for (var i = 0; i < changedProperties.length; ++i)
                elementsPanel.addStyleChange(section.identifier, self.style, changedProperties[i]);
            self._styleRule.style = self.style;
            if (section && section.pane)
                section.pane.dispatchEventToListeners("style edited");

            if (updateInterface)
                self.updateAll(true);

            if (!self.rule)
                WebInspector.panels.elements.treeOutline.update();
        };
        InspectorController.applyStyleText(this.style._id, styleText.trimWhitespace(), this.name, callback);
    }
}

WebInspector.StylePropertyTreeElement.prototype.__proto__ = TreeElement.prototype;
