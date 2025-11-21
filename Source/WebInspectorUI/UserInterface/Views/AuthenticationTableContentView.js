/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

WI.AuthenticationTableContentView = class AuthenticationTableContentView extends WI.ContentView
{
    constructor(representedObject, extraArguments)
    {
        super(representedObject);
        this._ceremonies = [];
        this._ceremoniesById = new Map;

        this._table = null;
        this._selectedObject = null;
        this._detailView = null;
        this._detailViewMap = new Map;

        this._siteColumnWidthSetting = new WI.Setting("authentication-table-content-view-ceremony-column-width", WI.Sidebar.AbsoluteMinimumWidth);

        this.element.classList.add("authentication-table");
        WI.Frame.addEventListener(WI.Frame.Event.DidStartWebAuthenticationOperation, this._frameDidStartWebAuthenticationOperation, this);
        WI.Frame.addEventListener(WI.Frame.Event.DidFinishWebAuthenticationOperation, this._frameDidFinishWebAuthenticationOperation, this);
    }

    // Public

    _frameDidStartWebAuthenticationOperation(event)
    {
        let ceremonyId = event.data.ceremonyId;

        let ceremony = {
            ceremonyId: ceremonyId,
            type: event?.data?.request?.rpId ? "get" : "create",
            request: event.data.request,
            response: null, // Will be filled in on finish
            initiatorStackTrace: event.data.initiatorStackTrace,
            initiatorSourceCodeLocation: event.data.initiatorSourceCodeLocation,
            initiatorNode: event.data.initiatorNode,
        };
        this._ceremonies.push(ceremony);
        this._ceremoniesById.set(ceremonyId, ceremony);
        this._table.reloadData();
    }

    _frameDidFinishWebAuthenticationOperation(event)
    {
        let ceremonyId = event.data.ceremonyId;
        let existingCeremony = this._ceremoniesById.get(ceremonyId);

        if (existingCeremony) {
            existingCeremony.response = event.data.response;

            let detailView = this._detailViewMap.get(existingCeremony);
            if (detailView) {
                detailView.showResponseTab();

                detailView._responseContentView = null;
            }

            this._table.reloadData();
        }
    }

    
    layout()
    {
        this._positionDetailView();
    }

    // Protected

    initialLayout()
    {
        super.initialLayout();
        this._siteColumn = new WI.TableColumn("site", WI.UIString("Ceremony"), {
            minWidth: this._siteColumnWidthSetting.defaultValue,
            maxWidth: 500,
            initialWidth: this._siteColumnWidthSetting.value,
            resizeType: WI.TableColumn.ResizeType.Locked,
        });
        this._typeColumn = new WI.TableColumn("type", WI.UIString("Type"), {
            minWidth: 50,
            maxWidth: 2000,
            initialWidth: 100,
        });
        this._userHandleColumn = new WI.TableColumn("user_handle", WI.UIString("User Handle"), {
            minWidth: 50,
            maxWidth: 2000,
            initialWidth: 100,
        });
        this._timeoutColumn = new WI.TableColumn("timeout", WI.UIString("Timeout"), {
            minWidth: 50,
            maxWidth: 2000,
            initialWidth: 100,
        });
        this._attachmentColumn = new WI.TableColumn("attachment", WI.UIString("Attachment"), {
            minWidth: 50,
            maxWidth: 2000,
            initialWidth: 100,
        });
        this._attestationColumn = new WI.TableColumn("attestation", WI.UIString("Attestation"), {
            minWidth: 50,
            maxWidth: 2000,
            initialWidth: 100,
        });
        this._extensionsColumn = new WI.TableColumn("extensions", WI.UIString("Extensions"), {
            minWidth: 50,
            maxWidth: 2000,
            initialWidth: 100,
        });
        this._sourceLocationColumn = new WI.TableColumn("source_location", WI.UIString("Source Location"), {
            minWidth: 100,
            maxWidth: 2000,
            initialWidth: 150,
        });
        this._table = new WI.Table("authentication-table", this, this, 20);
        this._table.addColumn(this._siteColumn);
        this._table.addColumn(this._typeColumn);
        this._table.addColumn(this._userHandleColumn);
        this._table.addColumn(this._timeoutColumn);
        this._table.addColumn(this._attachmentColumn);
        this._table.addColumn(this._attestationColumn);
        this._table.addColumn(this._extensionsColumn);
        this._table.addColumn(this._sourceLocationColumn);
        this._siteColumn.addEventListener(WI.TableColumn.Event.WidthDidChange, this._tableSiteColumnDidChangeWidth, this);
        this.addSubview(this._table);
    }

    // Table dataSource

    tableIndexForRepresentedObject(table, object)
    {
        return this._ceremonies.indexOf(object);
    }

    tableRepresentedObjectForIndex(table, index)
    {
        return this._ceremonies[index];
    }

    tableNumberOfRows(table)
    {
        return this._ceremonies.length;
    }
    
    tableSortChanged(table)
    {
        this._table.reloadData();
    }

    // Table delegate

    tableShouldSelectRow(table, cell, column, rowIndex)
    {
        return column === this._siteColumn;
    }

    tableCellContextMenuClicked(table, cell, column, rowIndex, event)
    {
        this._table.selectRow(rowIndex);

        let contextMenu = WI.ContextMenu.createFromEvent(event);
        let ceremony = this._ceremonies[rowIndex];

        // Only show copy options for the site column
        if (column === this._siteColumn) {
            contextMenu.appendItem(WI.UIString("Copy"), () => {
                let rowIndexes;
                if (table.isRowSelected(rowIndex))
                    rowIndexes = table.selectedRows;
                else
                    rowIndexes = [rowIndex];

                let ceremonies = rowIndexes.map(index => this._ceremonies[index]);
                let text = ceremonies.map(ceremony => {
                    let site = ceremony?.request?.rp?.id || ceremony?.request?.rpId || "";
                    let type = ceremony?.type || "";
                    let userHandle = ceremony?.request?.user?.id || ceremony?.response?.response?.userHandle || "";
                    return `${site}\t${type}\t${userHandle}`;
                }).join("\n");
                InspectorFrontendHost.copyText(text);
            });

            contextMenu.appendSeparator();

            contextMenu.appendItem(WI.UIString("Copy Request as JSON"), () => {
                let request = ceremony?.request || {};
                let jsonString = JSON.stringify(request, null, 2);
                InspectorFrontendHost.copyText(jsonString);
            });

            contextMenu.appendItem(WI.UIString("Copy Response as JSON"), () => {
                let response = ceremony?.response || {};
                let jsonString = JSON.stringify(response, null, 2);
                InspectorFrontendHost.copyText(jsonString);
            });

            contextMenu.appendSeparator();
        }
    }


    tablePopulateCell(table, cell, column, rowIndex)
    {
        let row = this._ceremonies[rowIndex];
        
        let cellText = "";
        switch (column.identifier) {
        case "site":
            let site = row?.request?.rp?.id || row?.request?.rpId || WI.UIString("Unknown");
            let type = row?.type === "create" ? WI.UIString("Creation") : WI.UIString("Assertion");
            let attachment = row?.request?.authenticatorSelection?.authenticatorAttachment || row?.response?.authenticatorAttachment || "unknown";
            let authenticatorType = attachment === "platform" ? WI.UIString("Platform") : attachment === "cross-platform" ? WI.UIString("Cross-Platform") : WI.UIString("Unknown");
            cellText = `${site} - ${type} (${authenticatorType})`;
            break;
        case "type":
            cellText = row?.type || WI.UIString("N/A");
            break;
        case "user_handle":
            cellText = row?.request?.user?.id || row?.response?.response?.userHandle || WI.UIString("N/A");
            break;
        case "timeout":
            cellText = row?.request?.timeout || WI.UIString("N/A");
            break;
        case "attachment":
            cellText = row?.request?.authenticatorSelection?.authenticatorAttachment || row?.response?.authenticatorAttachment || WI.UIString("N/A");
            break;
        case "attestation":
            cellText = row?.request?.attestation || WI.UIString("N/A");
            break;
        case "extensions":
            cellText = Object.keys(row?.request?.extensions || {}).join(", ") || WI.UIString("None");
            break;
        case "source_location":
            if (row?.initiatorSourceCodeLocation) {
                let location = row.initiatorSourceCodeLocation;
                cell.removeChildren();
                let linkElement = WI.createSourceCodeLocationLink(location, {
                    dontFloat: true,
                    ignoreSearchTab: true,
                });
                if (linkElement) {
                    cell.appendChild(linkElement);
                    return cell;
                }
            } else {
                cellText = "—";
            }
            break;
        default:
            cellText = WI.UIString("Unknown");
        }
        
        cell.textContent = cellText;
        return cell;
    }

    tableSelectionDidChange(table)
    {
        let rowIndex = table.selectedRow;
        if (isNaN(rowIndex)) {
            this._selectedObject = null;
            this._hideDetailView();
            return;
        }
        let ceremony = this._ceremonies[rowIndex];
        if (ceremony == this._selectedObject)
            return;

        this._selectedObject = ceremony;
        if (this._selectedObject)
            this._showDetailView(this._selectedObject);
        else
            this._hideDetailView();
    }

    // Private

    _hideDetailView()
    {
        if (!this._detailView)
            return;

        this.element.classList.remove("showing-detail");
        this._table.scrollContainer.style.removeProperty("width");

        this.removeSubview(this._detailView);
        this._detailView = null;

        this._table.updateLayout(WI.View.LayoutReason.Resize);
        this._table.reloadData();
    }

    _showDetailView(object)
    {
        let oldDetailView = this._detailView;

        this._detailView = this._detailViewMap.get(object);
        if (this._detailView === oldDetailView)
            return;

        if (!this._detailView) {
            this._detailView = new WI.AuthenticationCeremonyDetailView(object, this);
            this._detailViewMap.set(object, this._detailView);
        }

        if (oldDetailView)
            this.replaceSubview(oldDetailView, this._detailView);
        else
            this.addSubview(this._detailView);

        this.element.classList.add("showing-detail");
        this._table.scrollContainer.style.width = this._siteColumn.width + "px";

        this.updateLayout();
        
        this._table.reloadVisibleColumnCells(this._siteColumn);
    }

    _tableSiteColumnDidChangeWidth(event)
    {
        this._siteColumnWidthSetting.value = event.target.width;
        this._positionDetailView();
    }

    
    _positionDetailView()
    {
        if (!this._detailView)
            return;

        let side = WI.resolvedLayoutDirection() === WI.LayoutDirection.RTL ? "right" : "left";
        this._detailView.element.style[side] = this._siteColumn.width + "px";
        this._table.scrollContainer.style.width = this._siteColumn.width + "px";
    }

    // AuthenticationCeremonyDetailView delegate

    authenticationDetailViewClose(authenticationDetailView)
    {
        this._selectedObject = null;
        this._table.deselectAll();
        this._hideDetailView();
    }

    closed()
    {
        super.closed();

        WI.Frame.removeEventListener(WI.Frame.Event.DidStartWebAuthenticationOperation, this._frameDidStartWebAuthenticationOperation, this);
        WI.Frame.removeEventListener(WI.Frame.Event.DidFinishWebAuthenticationOperation, this._frameDidFinishWebAuthenticationOperation, this);
    }
};
