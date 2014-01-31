/*
 * Copyright (C) 2009 280 North Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

WebInspector.LegacyTopDownProfileDataGridNode = function(profileNode, owningTree)
{
    var hasChildren = (profileNode.children && profileNode.children.length);

    WebInspector.LegacyProfileDataGridNode.call(this, profileNode, owningTree, hasChildren);

    this._remainingChildren = profileNode.children;
}

WebInspector.LegacyTopDownProfileDataGridNode.prototype = {
    _sharedPopulate: function()
    {
        var children = this._remainingChildren;
        var childrenLength = children.length;

        for (var i = 0; i < childrenLength; ++i)
            this.appendChild(new WebInspector.LegacyTopDownProfileDataGridNode(children[i], this.tree, false, this.showTimeAsPercent));

        this._remainingChildren = null;
    },

    _exclude: function(aCallUID)
    {
        if (this._remainingChildren)
            this._populate();

        this._save();

        var children = this.children;
        var index = this.children.length;

        while (index--)
            children[index]._exclude(aCallUID);

        var child = this.childrenByCallUID[aCallUID];

        if (child)
            this._merge(child, true);
    }
}

WebInspector.LegacyTopDownProfileDataGridNode.prototype.__proto__ = WebInspector.LegacyProfileDataGridNode.prototype;

WebInspector.LegacyTopDownProfileDataGridTree = function(profileNode)
{
    WebInspector.LegacyProfileDataGridTree.call(this, profileNode);

    this._remainingChildren = profileNode.children;

    WebInspector.LegacyTopDownProfileDataGridNode.prototype._populate.call(this);
}

WebInspector.LegacyTopDownProfileDataGridTree.prototype = {
    focus: function(profileDataGrideNode)
    {
        if (!profileDataGrideNode)
            return;

        this._save();
        profileDataGrideNode.savePosition();

        this.children = [profileDataGrideNode];
        this.totalTime = profileDataGrideNode.totalTime;
    },

    exclude: function(profileDataGrideNode)
    {
        if (!profileDataGrideNode)
            return;

        this._save();

        var excludedCallUID = profileDataGrideNode.callUID;

        WebInspector.LegacyTopDownProfileDataGridNode.prototype._exclude.call(this, excludedCallUID);

        if (this.lastComparator)
            this.sort(this.lastComparator, true);
    },

    restore: function()
    {
        if (!this._savedChildren)
            return;

        this.children[0].restorePosition();

        WebInspector.LegacyProfileDataGridTree.prototype.restore.call(this);
    },

    _merge: WebInspector.LegacyTopDownProfileDataGridNode.prototype._merge,

    _sharedPopulate: WebInspector.LegacyTopDownProfileDataGridNode.prototype._sharedPopulate
}

WebInspector.LegacyTopDownProfileDataGridTree.prototype.__proto__ = WebInspector.LegacyProfileDataGridTree.prototype;
