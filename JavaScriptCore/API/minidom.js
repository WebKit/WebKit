// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

// For browser-based testing
if (window) {
    /* 
    The methods and constructors below approximate what that the native host should provide

    print(string);

    Node
        |-- TextNode
        |-- Element
            |-- RootElement
            |-- HeroElement
            |-- VillainElement
            |-- NameElement
            |-- WeaponElement
        |-- Document
    */
      
    function print(string, indentLevel)
    {
        document.getElementById('pre').appendChild(document.createTextNode(string));
    }
    
    Node = function()
    {
        this.__defineGetter__("childNodes", function() {
            if (!this._childNodes)
                this._childNodes = new Array();
            return this._childNodes;
        });
        this.__defineGetter__("firstChild", function () { 
            return this.childNodes[0];
        });
    }

    Node.prototype.tag = "Node";

    Node.prototype.appendChild = function(child) {
        this.childNodes.push(child);
    }
    
    Node.prototype.serialize = function(numSpaces) {
        function printSpaces(n) 
        {
            for (var i = 0; i < n; i++) // >
            print(" ");
        }

        printSpaces(numSpaces);
        print('<' + this.tag + '>' + '\n');
        
        var childNodesLength = this.childNodes.length;
        for (var i = 0; i < childNodesLength; i++) //>
            this.childNodes[i].serialize(numSpaces + 4);
        
        printSpaces(numSpaces);
        print('</' + this.tag + '>\n');
    }

    TextNode = function(text)
    {
        this.text = text;
    }

    TextNode.prototype = new Node();
    
    TextNode.prototype.tag = "Text";
    
    TextNode.prototype.serialize = function(numSpaces) {
        for (var i = 0; i < numSpaces; i++) // >
            print(" ");
        print(this.text + '\n');
    }

    Element = function()
    {
    }
    
    Element.prototype = new Node();
    
    Element.prototype.tag = "Element";

    RootElement = function()
    {
    }
    
    RootElement.prototype = new Element();

    RootElement.prototype.tag = "Root";
    
    HeroElement = function()
    {
    }
    
    HeroElement.prototype = new Element();

    HeroElement.prototype.tag = "Hero";

    VillainElement = function()
    {
    }
    
    VillainElement.prototype = new Element();

    VillainElement.prototype.tag = "Villain";

    NameElement = function()
    {
    }
    
    NameElement.prototype = new Element();

    NameElement.prototype.tag = "Name";

    WeaponElement = function()
    {
    }
    
    WeaponElement.prototype = new Element();

    WeaponElement.prototype.tag = "Weapon";

    Document = function()
    {
    }

    Document.prototype = new Node();

    Document.prototype.serialize = function() {
        this.firstChild.serialize(0);
    }
    
    Document.prototype.createElement = function(elementType) {
        return eval('new ' + elementType + 'Element()');
    }

    Document.prototype.createTextNode = function(text) {
        return new TextNode(text);
    }
}

function test()
{
    var element, name, weapon;
    
    var document = new Document();
    document.appendChild(document.createElement('Root'));

    /* Tank Girl */
    element = document.createElement('Hero');

    name = document.createElement('Name');
    name.appendChild(document.createTextNode('Tank Girl'));
    element.appendChild(name);
    
    weapon = document.createElement('Weapon');
    weapon.appendChild(document.createTextNode('Tank'));
    element.appendChild(weapon);
    
    weapon = document.createElement('Weapon');
    weapon.appendChild(document.createTextNode('Attitude'));
    element.appendChild(weapon);
    
    weapon = document.createElement('Weapon');
    weapon.appendChild(document.createTextNode('Army of genetically engineered super-kangaroos'));
    element.appendChild(weapon);
    
    document.firstChild.appendChild(element);


    /* Skeletor */
    element = document.createElement('Villain');

    name = document.createElement('Name');
    name.appendChild(document.createTextNode('Skeletor'));
    element.appendChild(name);
    
    weapon = document.createElement('Weapon');
    weapon.appendChild(document.createTextNode('Havok Staff'));
    element.appendChild(weapon);
    
    weapon = document.createElement('Weapon');
    weapon.appendChild(document.createTextNode('Motley crew of henchmen'));
    element.appendChild(weapon);
    
    document.firstChild.appendChild(element);

    /* Serialize */
    document.serialize();
}
