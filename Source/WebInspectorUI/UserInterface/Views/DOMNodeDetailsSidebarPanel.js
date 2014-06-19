/*
 * Copyright (C) 2013, 2014 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

WebInspector.DOMNodeDetailsSidebarPanel = function() {
    WebInspector.DOMDetailsSidebarPanel.call(this, "dom-node-details", WebInspector.UIString("Node"), WebInspector.UIString("Node"), "Images/NavigationItemAngleBrackets.svg", "2");

    WebInspector.domTreeManager.addEventListener(WebInspector.DOMTreeManager.Event.AttributeModified, this._attributesChanged, this);
    WebInspector.domTreeManager.addEventListener(WebInspector.DOMTreeManager.Event.AttributeRemoved, this._attributesChanged, this);

    this.element.classList.add(WebInspector.DOMNodeDetailsSidebarPanel.StyleClassName);

    this._identityNodeTypeRow = new WebInspector.DetailsSectionSimpleRow(WebInspector.UIString("Type"));
    this._identityNodeNameRow = new WebInspector.DetailsSectionSimpleRow(WebInspector.UIString("Name"));
    this._identityNodeValueRow = new WebInspector.DetailsSectionSimpleRow(WebInspector.UIString("Value"));

    var identityGroup = new WebInspector.DetailsSectionGroup([this._identityNodeTypeRow, this._identityNodeNameRow, this._identityNodeValueRow]);
    var identitySection = new WebInspector.DetailsSection("dom-node-identity", WebInspector.UIString("Identity"), [identityGroup]);

    this._attributesDataGridRow = new WebInspector.DetailsSectionDataGridRow(null, WebInspector.UIString("No Attributes"));

    var attributesGroup = new WebInspector.DetailsSectionGroup([this._attributesDataGridRow]);
    var attributesSection = new WebInspector.DetailsSection("dom-node-attributes", WebInspector.UIString("Attributes"), [attributesGroup]);

    this._propertiesRow = new WebInspector.DetailsSectionRow;

    var propertiesGroup = new WebInspector.DetailsSectionGroup([this._propertiesRow]);
    var propertiesSection = new WebInspector.DetailsSection("dom-node-properties", WebInspector.UIString("Properties"), [propertiesGroup]);

    this._eventListenersSectionGroup = new WebInspector.DetailsSectionGroup;
    var eventListenersSection = new WebInspector.DetailsSection("dom-node-event-listeners", WebInspector.UIString("Event Listeners"), [this._eventListenersSectionGroup]);    

    this.element.appendChild(identitySection.element);
    this.element.appendChild(attributesSection.element);
    this.element.appendChild(propertiesSection.element);
    this.element.appendChild(eventListenersSection.element);

    if (this._accessibilitySupported()) {
        this._accessibilityEmptyRow = new WebInspector.DetailsSectionRow(WebInspector.UIString("No Accessibility Information"));
        this._accessibilityNodeActiveDescendantRow = new WebInspector.DetailsSectionSimpleRow(WebInspector.UIString("Shared Focus"));
        this._accessibilityNodeBusyRow = new WebInspector.DetailsSectionSimpleRow(WebInspector.UIString("Busy"));
        this._accessibilityNodeCheckedRow = new WebInspector.DetailsSectionSimpleRow(WebInspector.UIString("Checked"));
        this._accessibilityNodeChildrenRow = new WebInspector.DetailsSectionSimpleRow(WebInspector.UIString("Children"));
        this._accessibilityNodeControlsRow = new WebInspector.DetailsSectionSimpleRow(WebInspector.UIString("Controls"));
        this._accessibilityNodeDisabledRow = new WebInspector.DetailsSectionSimpleRow(WebInspector.UIString("Disabled"));
        this._accessibilityNodeExpandedRow = new WebInspector.DetailsSectionSimpleRow(WebInspector.UIString("Expanded"));
        this._accessibilityNodeFlowsRow = new WebInspector.DetailsSectionSimpleRow(WebInspector.UIString("Flows"));
        this._accessibilityNodeFocusedRow = new WebInspector.DetailsSectionSimpleRow(WebInspector.UIString("Focused"));
        this._accessibilityNodeIgnoredRow = new WebInspector.DetailsSectionSimpleRow(WebInspector.UIString("Ignored"));
        this._accessibilityNodeInvalidRow = new WebInspector.DetailsSectionSimpleRow(WebInspector.UIString("Invalid"));
        this._accessibilityNodeLiveRegionStatusRow = new WebInspector.DetailsSectionSimpleRow(WebInspector.UIString("Live"));
        this._accessibilityNodeMouseEventRow = new WebInspector.DetailsSectionSimpleRow("");
        this._accessibilityNodeLabelRow = new WebInspector.DetailsSectionSimpleRow(WebInspector.UIString("Label"));
        this._accessibilityNodeOwnsRow = new WebInspector.DetailsSectionSimpleRow(WebInspector.UIString("Owns"));
        this._accessibilityNodeParentRow = new WebInspector.DetailsSectionSimpleRow(WebInspector.UIString("Parent"));
        this._accessibilityNodePressedRow = new WebInspector.DetailsSectionSimpleRow(WebInspector.UIString("Pressed"));
        this._accessibilityNodeReadonlyRow = new WebInspector.DetailsSectionSimpleRow(WebInspector.UIString("Readonly"));
        this._accessibilityNodeRequiredRow = new WebInspector.DetailsSectionSimpleRow(WebInspector.UIString("Required"));
        this._accessibilityNodeRoleRow = new WebInspector.DetailsSectionSimpleRow(WebInspector.UIString("Role"));
        this._accessibilityNodeSelectedRow = new WebInspector.DetailsSectionSimpleRow(WebInspector.UIString("Selected"));
        this._accessibilityNodeSelectedChildrenRow = new WebInspector.DetailsSectionSimpleRow(WebInspector.UIString("Selected Items"));
    
        this._accessibilityGroup = new WebInspector.DetailsSectionGroup([this._accessibilityEmptyRow]);
        var accessibilitySection = new WebInspector.DetailsSection("dom-node-accessibility", WebInspector.UIString("Accessibility"), [this._accessibilityGroup]);    

        this.element.appendChild(accessibilitySection.element);
    }
};

WebInspector.DOMNodeDetailsSidebarPanel.StyleClassName = "dom-node";
WebInspector.DOMNodeDetailsSidebarPanel.PropertiesObjectGroupName = "dom-node-details-sidebar-properties-object-group";

WebInspector.DOMNodeDetailsSidebarPanel.prototype = {
    constructor: WebInspector.DOMNodeDetailsSidebarPanel,

    // Public

    refresh: function()
    {
        var domNode = this.domNode;
        if (!domNode)
            return;

        this._identityNodeTypeRow.value = this._nodeTypeDisplayName();
        this._identityNodeNameRow.value = domNode.nodeNameInCorrectCase();
        this._identityNodeValueRow.value = domNode.nodeValue();

        this._refreshAttributes();
        this._refreshProperties();
        this._refreshEventListeners();
        this._refreshAccessibility();
    },

    // Private

    _accessibilitySupported: function()
    {
        return window.DOMAgent && DOMAgent.getAccessibilityPropertiesForNode;
    },

    _refreshAttributes: function()
    {
        this._attributesDataGridRow.dataGrid = this._createAttributesDataGrid();
    },

    _refreshProperties: function()
    {
        var domNode = this.domNode;
        if (!domNode)
            return;

        RuntimeAgent.releaseObjectGroup(WebInspector.DOMNodeDetailsSidebarPanel.PropertiesObjectGroupName);
        WebInspector.RemoteObject.resolveNode(domNode, WebInspector.DOMNodeDetailsSidebarPanel.PropertiesObjectGroupName, nodeResolved.bind(this));

        function nodeResolved(object)
        {
            if (!object)
                return;

            // Bail if the DOM node changed while we were waiting for the async response.
            if (this.domNode !== domNode)
                return;

            function collectPrototypes()
            {
                // This builds an object with numeric properties. This is easier than dealing with arrays
                // with the way RemoteObject works. Start at 1 since we use parseInt later and parseInt
                // returns 0 for non-numeric strings make it ambiguous.
                var prototype = this;
                var result = [];
                var counter = 1;

                while (prototype) {
                    result[counter++] = prototype;
                    prototype = prototype.__proto__;
                }

                return result;
            }

            object.callFunction(collectPrototypes, undefined, nodePrototypesReady.bind(this));
            object.release();
        }

        function nodePrototypesReady(object)
        {
            if (!object)
                return;

            // Bail if the DOM node changed while we were waiting for the async response.
            if (this.domNode !== domNode)
                return;

            object.getOwnProperties(fillSection.bind(this));
        }

        function fillSection(prototypes)
        {
            if (!prototypes)
                return;

            // Bail if the DOM node changed while we were waiting for the async response.
            if (this.domNode !== domNode)
                return;

            var element = this._propertiesRow.element;
            element.removeChildren();

            // Get array of prototype user-friendly names.
            for (var i = 0; i < prototypes.length; ++i) {
                // The only values we care about are numeric, as assigned in collectPrototypes.
                if (!parseInt(prototypes[i].name, 10))
                    continue;

                var prototype = prototypes[i].value;
                var title = prototype.description;
                if (title.match(/Prototype$/))
                    title = title.replace(/Prototype$/, WebInspector.UIString(" (Prototype)"));
                else if (title === "Object")
                    title = title + WebInspector.UIString(" (Prototype)");

                var propertiesSection = new WebInspector.ObjectPropertiesSection(prototype);

                var detailsSection = new WebInspector.DetailsSection(prototype.description.hash + "-prototype-properties", title, null, null, true);
                detailsSection.groups[0].rows = [new WebInspector.DetailsSectionPropertiesRow(propertiesSection)];

                element.appendChild(detailsSection.element);
            }
        }
    },

    _refreshEventListeners: function()
    {
        var domNode = this.domNode;
        if (!domNode)
            return;

        domNode.eventListeners(eventListenersCallback.bind(this));

        function eventListenersCallback(error, eventListeners)
        {
            if (error)
                return;

            // Bail if the DOM node changed while we were waiting for the async response.
            if (this.domNode !== domNode)
                return;

            var eventListenerTypes = [];
            var eventListenerSections = {};
            for (var i = 0; i < eventListeners.length; ++i) {
                var eventListener = eventListeners[i];
                eventListener.node = WebInspector.domTreeManager.nodeForId(eventListener.nodeId);

                var type = eventListener.type;
                var section = eventListenerSections[type];
                if (!section) {
                    section = new WebInspector.EventListenerSection(type, domNode.id);
                    eventListenerSections[type] = section;
                    eventListenerTypes.push(type);
                }

                section.addListener(eventListener);
            }

            if (!eventListenerTypes.length) {
                var emptyRow = new WebInspector.DetailsSectionRow(WebInspector.UIString("No Event Listeners"));
                emptyRow.showEmptyMessage();
                this._eventListenersSectionGroup.rows = [emptyRow];
                return;
            }

            eventListenerTypes.sort();

            var rows = [];
            for (var i = 0; i < eventListenerTypes.length; ++i)
                rows.push(eventListenerSections[eventListenerTypes[i]]);
            this._eventListenersSectionGroup.rows = rows;
        }
    },

    _refreshAccessibility: (function() {
        var properties = {};
        var domNode;

        function booleanValueToLocalizedStringIfTrue(property) {
            if (properties[property])
                return WebInspector.UIString("Yes");
            return "";
        }

        function booleanValueToLocalizedStringIfPropertyDefined(property) {
            if (properties[property] !== undefined) {
                if (properties[property])
                    return WebInspector.UIString("Yes");
                else
                    return WebInspector.UIString("No");
            }
            return "";
        }

        function linkForNodeId(nodeId) {
            var link = null;
            if (nodeId !== undefined && typeof nodeId === "number") {
                var node = WebInspector.domTreeManager.nodeForId(nodeId);
                if (node)
                    link = WebInspector.linkifyAccessibilityNodeReference(node);
            }
            return link;
        }

        function linkListForNodeIds(nodeIds) {
            var hasLinks = false;
            var linkList = null;
            if (nodeIds !== undefined) {
                linkList = document.createElement("ul");
                linkList.className = "node-link-list";    
                for (var nodeId of nodeIds) {
                    var node = WebInspector.domTreeManager.nodeForId(nodeId);
                    if (node) {
                        var link = WebInspector.linkifyAccessibilityNodeReference(node);
                        if (link) {
                            hasLinks = true;
                            var listitem = document.createElement("li");
                            listitem.appendChild(link);
                            linkList.appendChild(listitem);
                        }
                    }
                }
            }
            return hasLinks ? linkList : null;
        }

        function accessibilityPropertiesCallback(accessibilityProperties) {
            if (this.domNode !== domNode)
                return;

            // Make sure the current set of properties is available in the closure scope for the helper functions.
            properties = accessibilityProperties;

            if (accessibilityProperties && accessibilityProperties.exists) {

                var activeDescendantLink = linkForNodeId(accessibilityProperties.activeDescendantNodeId);
                var busy = booleanValueToLocalizedStringIfPropertyDefined("busy");

                var checked = "";
                if (accessibilityProperties.checked !== undefined) {
                    if (accessibilityProperties.checked === DOMAgent.AccessibilityPropertiesChecked.True)
                        checked = WebInspector.UIString("Yes");
                    else if (accessibilityProperties.checked === DOMAgent.AccessibilityPropertiesChecked.Mixed)
                        checked = WebInspector.UIString("Mixed");
                    else // DOMAgent.AccessibilityPropertiesChecked.False
                        checked = WebInspector.UIString("No");
                }

                // Accessibility tree children are not a 1:1 mapping with DOM tree children.
                var childNodeLinkList = linkListForNodeIds(accessibilityProperties.childNodeIds);
                
                var controlledNodeLinkList = linkListForNodeIds(accessibilityProperties.controlledNodeIds);
                var disabled = booleanValueToLocalizedStringIfTrue("disabled");
                var expanded = booleanValueToLocalizedStringIfPropertyDefined("expanded");
                var flowedNodeLinkList = linkListForNodeIds(accessibilityProperties.flowedNodeIds);
                var focused = booleanValueToLocalizedStringIfPropertyDefined("focused");
                
                var ignored = "";
                if (accessibilityProperties.ignored) {
                    ignored = WebInspector.UIString("Yes");
                    if (accessibilityProperties.hidden)
                        ignored = WebInspector.UIString("%s (hidden)").format(ignored);
                    else if (accessibilityProperties.ignoredByDefault)
                        ignored = WebInspector.UIString("%s (default)").format(ignored);
                }

                var invalid = "";
                if (accessibilityProperties.invalid === DOMAgent.AccessibilityPropertiesInvalid.True)
                    invalid = WebInspector.UIString("Yes");
                else if (accessibilityProperties.invalid === DOMAgent.AccessibilityPropertiesInvalid.Grammar)
                    invalid = WebInspector.UIString("Grammar");
                else if (accessibilityProperties.invalid === DOMAgent.AccessibilityPropertiesInvalid.Spelling)
                    invalid = WebInspector.UIString("Spelling");

                var liveRegionStatus = "";
                var liveRegionStatusNode = null;
                var liveRegionStatusToken = accessibilityProperties.liveRegionStatus;
                switch(liveRegionStatusToken) {
                case DOMAgent.AccessibilityPropertiesLiveRegionStatus.Assertive:
                    liveRegionStatus = WebInspector.UIString("Assertive");
                    break;
                case DOMAgent.AccessibilityPropertiesLiveRegionStatus.Polite:
                    liveRegionStatus = WebInspector.UIString("Polite");
                    break;
                default:
                    liveRegionStatus = "";
                }
                if (liveRegionStatus) {
                    var liveRegionRelevant = accessibilityProperties.liveRegionRelevant;
                    // Append @aria-relevant values. E.g. "Live: Assertive (Additions, Text)".
                    if (liveRegionRelevant && liveRegionRelevant.length) {
                        // @aria-relevant="all" is exposed as ["additions","removals","text"], in order.
                        // This order is controlled in WebCore and expected in WebInspectorUI.
                        if (liveRegionRelevant.length === 3 
                            && liveRegionRelevant[0] === DOMAgent.LiveRegionRelevant.Additions
                            && liveRegionRelevant[1] === DOMAgent.LiveRegionRelevant.Removals
                            && liveRegionRelevant[2] === DOMAgent.LiveRegionRelevant.Text)
                            liveRegionRelevant = [WebInspector.UIString("All Changes")];
                        else {
                            // Reassign localized strings in place: ["additions","text"] becomes ["Additions","Text"].
                            liveRegionRelevant = liveRegionRelevant.map(function(value) {
                                switch (value) {
                                case DOMAgent.LiveRegionRelevant.Additions:
                                    return WebInspector.UIString("Additions");
                                case DOMAgent.LiveRegionRelevant.Removals:
                                    return WebInspector.UIString("Removals");
                                case DOMAgent.LiveRegionRelevant.Text:
                                    return WebInspector.UIString("Text");
                                default: // If WebCore sends a new unhandled value, display as a String.
                                    return "\"" + value + "\"";
                                }
                            });
                        }
                        liveRegionStatus += " (" + liveRegionRelevant.join(", ") + ")";
                    }
                    // Clarify @aria-atomic if necessary.
                    if (accessibilityProperties.liveRegionAtomic) {
                        liveRegionStatusNode = document.createElement("div");
                        liveRegionStatusNode.className = "value-with-clarification";
                        liveRegionStatusNode.setAttribute("role", "text");
                        liveRegionStatusNode.appendChild(document.createTextNode(liveRegionStatus));
                        var clarificationNode = document.createElement("div");
                        clarificationNode.className = "clarification";
                        clarificationNode.appendChild(document.createTextNode(WebInspector.UIString("Region announced in its entirety.")));
                        liveRegionStatusNode.appendChild(clarificationNode);
                    }
                }

                var mouseEventNodeId = accessibilityProperties.mouseEventNodeId;
                var mouseEventTextValue = "";
                var mouseEventNodeLink = null;
                if (mouseEventNodeId) {
                    if (mouseEventNodeId === accessibilityProperties.nodeId)
                        mouseEventTextValue = WebInspector.UIString("Yes");
                    else
                        mouseEventNodeLink = linkForNodeId(mouseEventNodeId);
                }

                // FIXME: label will always come back as empty. Blocked by http://webkit.org/b/121134
                var label = accessibilityProperties.label;
                if (label && label !== domNode.getAttribute("aria-label"))
                    label = WebInspector.UIString("%s (computed)").format(label);

                var ownedNodeLinkList = linkListForNodeIds(accessibilityProperties.ownedNodeIds);

                // Accessibility tree parent is not a 1:1 mapping with the DOM tree parent.
                var parentNodeLink = linkForNodeId(accessibilityProperties.parentNodeId);

                var pressed = booleanValueToLocalizedStringIfPropertyDefined("pressed");
                var readonly = booleanValueToLocalizedStringIfTrue("readonly");
                var required = booleanValueToLocalizedStringIfPropertyDefined("required");

                var role = accessibilityProperties.role;
                if (role === "" || role === "unknown")
                    role = WebInspector.UIString("No exact ARIA role match.");
                else if (role) {
                    if (!domNode.getAttribute("role"))
                        role = WebInspector.UIString("%s (default)").format(role);
                    else if (domNode.getAttribute("role") !== role)
                        role = WebInspector.UIString("%s (computed)").format(role);
                }

                var selected = booleanValueToLocalizedStringIfTrue("selected");
                var selectedChildNodeLinkList = linkListForNodeIds(accessibilityProperties.selectedChildNodeIds);

                // Assign all the properties to their respective views.
                this._accessibilityNodeActiveDescendantRow.value = activeDescendantLink || "";
                this._accessibilityNodeBusyRow.value = busy;
                this._accessibilityNodeCheckedRow.value = checked;
                this._accessibilityNodeChildrenRow.value = childNodeLinkList || "";
                this._accessibilityNodeControlsRow.value = controlledNodeLinkList || "";
                this._accessibilityNodeDisabledRow.value = disabled;
                this._accessibilityNodeExpandedRow.value = expanded;
                this._accessibilityNodeFlowsRow.value = flowedNodeLinkList || "";
                this._accessibilityNodeFocusedRow.value = focused;
                this._accessibilityNodeIgnoredRow.value = ignored;
                this._accessibilityNodeInvalidRow.value = invalid;
                this._accessibilityNodeLabelRow.value = label;
                this._accessibilityNodeLiveRegionStatusRow.value = liveRegionStatusNode || liveRegionStatus;
                
                // Row label changes based on whether the value is a delegate node link.
                this._accessibilityNodeMouseEventRow.label = mouseEventNodeLink ? WebInspector.UIString("Click Listener") : WebInspector.UIString("Clickable");
                this._accessibilityNodeMouseEventRow.value = mouseEventNodeLink || mouseEventTextValue;

                this._accessibilityNodeOwnsRow.value = ownedNodeLinkList || "";
                this._accessibilityNodeParentRow.value = parentNodeLink || "";
                this._accessibilityNodePressedRow.value = pressed;
                this._accessibilityNodeReadonlyRow.value = readonly;
                this._accessibilityNodeRequiredRow.value = required;
                this._accessibilityNodeRoleRow.value = role;
                this._accessibilityNodeSelectedRow.value = selected;

                this._accessibilityNodeSelectedChildrenRow.label = WebInspector.UIString("Selected Items");
                this._accessibilityNodeSelectedChildrenRow.value = selectedChildNodeLinkList || "";
                if (selectedChildNodeLinkList && accessibilityProperties.selectedChildNodeIds.length === 1)
                    this._accessibilityNodeSelectedChildrenRow.label = WebInspector.UIString("Selected Item");                

                // Display order, not alphabetical as above.
                this._accessibilityGroup.rows = [
                    // Global properties for all elements.
                    this._accessibilityNodeIgnoredRow,
                    this._accessibilityNodeRoleRow,
                    this._accessibilityNodeLabelRow,
                    this._accessibilityNodeParentRow,
                    this._accessibilityNodeActiveDescendantRow,
                    this._accessibilityNodeSelectedChildrenRow,
                    this._accessibilityNodeChildrenRow,
                    this._accessibilityNodeOwnsRow,
                    this._accessibilityNodeControlsRow,
                    this._accessibilityNodeFlowsRow,
                    this._accessibilityNodeMouseEventRow,
                    this._accessibilityNodeFocusedRow,
                    this._accessibilityNodeBusyRow,
                    this._accessibilityNodeLiveRegionStatusRow,

                    // Properties exposed for all input-type elements.
                    this._accessibilityNodeDisabledRow,
                    this._accessibilityNodeInvalidRow,
                    this._accessibilityNodeRequiredRow,

                    // Role-specific properties.
                    this._accessibilityNodeCheckedRow,
                    this._accessibilityNodeExpandedRow,
                    this._accessibilityNodePressedRow,
                    this._accessibilityNodeReadonlyRow,
                    this._accessibilityNodeSelectedRow
                ];

                this._accessibilityEmptyRow.hideEmptyMessage();

            } else {
                this._accessibilityGroup.rows = [this._accessibilityEmptyRow];
                this._accessibilityEmptyRow.showEmptyMessage();
            }
        }

        function refreshAX() {
            if (!this._accessibilitySupported())
                return;

            // Make sure the domNode is available in the closure scope.
            domNode = this.domNode;
            if (!domNode)
                return;

            domNode.accessibilityProperties(accessibilityPropertiesCallback.bind(this));
        }

        return refreshAX;
    }()),

    _attributesChanged: function(event)
    {
        if (event.data.node !== this.domNode)
            return;
        this._refreshAttributes();
        this._refreshAccessibility();
    },

    _nodeTypeDisplayName: function()
    {
        switch (this.domNode.nodeType()) {
        case Node.ELEMENT_NODE:
            return WebInspector.UIString("Element");
        case Node.TEXT_NODE:
            return WebInspector.UIString("Text Node");
        case Node.COMMENT_NODE:
            return WebInspector.UIString("Comment");
        case Node.DOCUMENT_NODE:
            return WebInspector.UIString("Document");
        case Node.DOCUMENT_TYPE_NODE:
            return WebInspector.UIString("Document Type");
        case Node.DOCUMENT_FRAGMENT_NODE:
            return WebInspector.UIString("Document Fragment");
        case Node.CDATA_SECTION_NODE:
            return WebInspector.UIString("Character Data");
        case Node.PROCESSING_INSTRUCTION_NODE:
            return WebInspector.UIString("Processing Instruction");
        default:
            console.error("Unknown DOM node type: ", this.domNode.nodeType());
            return this.domNode.nodeType();
        }
    },

    _createAttributesDataGrid: function()
    {
        var domNode = this.domNode;
        if (!domNode || !domNode.hasAttributes())
            return null;

        var columns = {name: {title: WebInspector.UIString("Name"), width: "30%"}, value: {title: WebInspector.UIString("Value")}};
        var dataGrid = new WebInspector.DataGrid(columns);

        var attributes = domNode.attributes();
        for (var i = 0; i < attributes.length; ++i) {
            var attribute = attributes[i];

            var node = new WebInspector.DataGridNode({name: attribute.name, value: attribute.value || ""}, false);
            node.selectable = true;

            dataGrid.appendChild(node);
        }

        return dataGrid;
    }
};

WebInspector.DOMNodeDetailsSidebarPanel.prototype.__proto__ = WebInspector.DOMDetailsSidebarPanel.prototype;
