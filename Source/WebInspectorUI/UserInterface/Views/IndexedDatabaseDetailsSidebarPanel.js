/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

WebInspector.IndexedDatabaseDetailsSidebarPanel = class IndexedDatabaseDetailsSidebarPanel extends WebInspector.DetailsSidebarPanel
{
    constructor()
    {
        super("indexed-database-details", WebInspector.UIString("Storage"), WebInspector.UIString("Storage"));

        this.element.classList.add("indexed-database");

        this._database = null;
        this._objectStore = null;
        this._index = null;

        this._databaseNameRow = new WebInspector.DetailsSectionSimpleRow(WebInspector.UIString("Name"));
        this._databaseVersionRow = new WebInspector.DetailsSectionSimpleRow(WebInspector.UIString("Version"));
        this._databaseSecurityOriginRow = new WebInspector.DetailsSectionSimpleRow(WebInspector.UIString("Security Origin"));
        this._databaseGroup = new WebInspector.DetailsSectionGroup([this._databaseNameRow, this._databaseVersionRow, this._databaseSecurityOriginRow]);
        this._databaseSection = new WebInspector.DetailsSection("indexed-database-database", WebInspector.UIString("Database"), [this._databaseGroup]);

        this._objectStoreNameRow = new WebInspector.DetailsSectionSimpleRow(WebInspector.UIString("Name"));
        this._objectStoreKeyPathRow = new WebInspector.DetailsSectionSimpleRow(WebInspector.UIString("Key Path"));
        this._objectStoreAutoIncrementRow = new WebInspector.DetailsSectionSimpleRow(WebInspector.UIString("Auto Increment"));
        this._objectStoreGroup = new WebInspector.DetailsSectionGroup([this._objectStoreNameRow, this._objectStoreKeyPathRow, this._objectStoreAutoIncrementRow]);
        this._objectStoreSection = new WebInspector.DetailsSection("indexed-database-object-store", WebInspector.UIString("Object Store"), [this._objectStoreGroup]);

        this._indexNameRow = new WebInspector.DetailsSectionSimpleRow(WebInspector.UIString("Name"));
        this._indexKeyPathRow = new WebInspector.DetailsSectionSimpleRow(WebInspector.UIString("Key Path"));
        this._indexUniqueRow = new WebInspector.DetailsSectionSimpleRow(WebInspector.UIString("Unique"));
        this._indexMultiEntryRow = new WebInspector.DetailsSectionSimpleRow(WebInspector.UIString("Multi-Entry"));
        this._indexGroup = new WebInspector.DetailsSectionGroup([this._indexNameRow, this._indexKeyPathRow, this._indexUniqueRow, this._indexMultiEntryRow]);
        this._indexSection = new WebInspector.DetailsSection("indexed-database-index", WebInspector.UIString("Index"), [this._indexGroup]);

        this.contentView.element.appendChild(this._databaseSection.element);
        this.contentView.element.appendChild(this._objectStoreSection.element);
        this.contentView.element.appendChild(this._indexSection.element);
    }

    // Public

    inspect(objects)
    {
        // Convert to a single item array if needed.
        if (!(objects instanceof Array))
            objects = [objects];

        this._database = null;
        this._objectStore = null;
        this._index = null;

        for (let object of objects) {
            if (object instanceof WebInspector.IndexedDatabase) {
                console.assert(!this._database, "Shouldn't have multiple IndexedDatabase objects in the list.");
                this._database = object;
            } else if (object instanceof WebInspector.IndexedDatabaseObjectStore) {
                console.assert(!this._database, "Shouldn't have multiple IndexedDatabase objects in the list.");
                this._objectStore = object;
                this._database = this._objectStore.parentDatabase;
            } else if (object instanceof WebInspector.IndexedDatabaseObjectStoreIndex) {
                console.assert(!this._database, "Shouldn't have multiple IndexedDatabase objects in the list.");
                this._index = object;
                this._objectStore = this._index.parentObjectStore;
                this._database = this._objectStore.parentDatabase;
            }
        }

        let hasObject = this._database || this._objectStore || this._index;
        if (hasObject)
            this.needsLayout();

        return hasObject;
    }

    // Protected

    layout()
    {
        if (!this._database)
            this._databaseSection.element.hidden = true;
        else {
            this._databaseSection.element.hidden = false;
            this._databaseSecurityOriginRow.value = this._database.securityOrigin;
            this._databaseNameRow.value = this._database.name;
            this._databaseVersionRow.value = this._database.version;
        }

        if (!this._objectStore)
            this._objectStoreSection.element.hidden = true;
        else {
            this._objectStoreSection.element.hidden = false;
            this._objectStoreNameRow.value = this._objectStore.name;
            this._objectStoreKeyPathRow.value = this._keyPathString(this._objectStore.keyPath);
            this._objectStoreAutoIncrementRow.value = this._objectStore.autoIncrement ? WebInspector.UIString("Yes") : WebInspector.UIString("No");
        }

        if (!this._index)
            this._indexSection.element.hidden = true;
        else {
            this._indexSection.element.hidden = false;
            this._indexNameRow.value = this._index.name;
            this._indexKeyPathRow.value = this._keyPathString(this._index.keyPath);
            this._indexUniqueRow.value = this._index.unique ? WebInspector.UIString("Yes") : WebInspector.UIString("No");

            if (this._index.keyPath.type !== IndexedDBAgent.KeyPathType.Array)
                this._indexMultiEntryRow.value = null;
            else
                this._indexMultiEntryRow.value = this._index.multiEntry ? WebInspector.UIString("Yes") : WebInspector.UIString("No");
        }
    }

    // Private

    _keyPathString(keyPath)
    {
        return keyPath ? JSON.stringify(keyPath) : emDash;
    }
};
