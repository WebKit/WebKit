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
if (typeof window != 'undefined') {
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

    Node.prototype.nodeType = "Node";

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
        print('<' + this.nodeType + '>' + '\n');
        
        var childNodesLength = this.childNodes.length;
        for (var i = 0; i < childNodesLength; i++) //>
            this.childNodes[i].serialize(numSpaces + 4);
        
        printSpaces(numSpaces);
        print('</' + this.nodeType + '>\n');
    }

    TextNode = function(text)
    {
        this.text = text;
    }

    TextNode.prototype = new Node();
    
    TextNode.prototype.nodeType = "Text";
    
    TextNode.prototype.serialize = function(numSpaces) {
        for (var i = 0; i < numSpaces; i++) // >
            print(" ");
        print(this.text + '\n');
    }

    Element = function()
    {
    }
    
    Element.prototype = new Node();
    
    Element.prototype.nodeType = "Element";

    RootElement = function()
    {
    }
    
    RootElement.prototype = new Element();

    RootElement.prototype.nodeType = "Root";
    
    HeroElement = function()
    {
    }
    
    HeroElement.prototype = new Element();

    HeroElement.prototype.nodeType = "Hero";

    VillainElement = function()
    {
    }
    
    VillainElement.prototype = new Element();

    VillainElement.prototype.nodeType = "Villain";

    NameElement = function()
    {
    }
    
    NameElement.prototype = new Element();

    NameElement.prototype.nodeType = "Name";

    WeaponElement = function()
    {
    }
    
    WeaponElement.prototype = new Element();

    WeaponElement.prototype.nodeType = "Weapon";

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

function shouldBe(a, b)
{
    var evalA;
    try {
        evalA = eval(a);
    } catch(e) {
        evalA = e;
    }
    
    if (evalA == b || isNaN(evalA) && typeof evalA == 'number' && isNaN(b) && typeof b == 'number')
        print("PASS: " + a + " should be " + b + " and is.", "green");
    else
        print("__FAIL__: " + a + " should be " + b + " but instead is " + evalA + ".", "red");
}

function test()
{
    print("Node is " + Node);
    for (var p in Node)
        print(p + ": " + Node[p]);
    
    node = new Node();
    print("node is " + node);
    for (var p in node)
        print(p + ": " + node[p]);

    child1 = new Node();
    child2 = new Node();
    child3 = new Node();
    
    node.appendChild(child1);
    node.appendChild(child2);

    for (var i = 0; i < node.childNodes.length + 1; i++) {
        print("item " + i + ": " + node.childNodes.item(i));
    }
    
    for (var i = 0; i < node.childNodes.length + 1; i++) {
        print(i + ": " + node.childNodes[i]);
    }

    node.removeChild(child1);
    node.replaceChild(child3, child2);
    
    for (var i = 0; i < node.childNodes.length + 1; i++) {
        print("item " + i + ": " + node.childNodes.item(i));
    }

    for (var i = 0; i < node.childNodes.length + 1; i++) {
        print(i + ": " + node.childNodes[i]);
    }

    try {
        node.appendChild(null);
    } catch(e) {
        print("caught: " + e);
    }
    
    try {
        var o = new Object();
        o.appendChild = node.appendChild;
        o.appendChild(node);
    } catch(e) {
        print("caught: " + e);
    }
    
    try {
        node.appendChild();
    } catch(e) {
        print("caught: " + e);
    }
    
    oldNodeType = node.nodeType;
    node.nodeType = 1;
    shouldBe("node.nodeType", oldNodeType);

    /*
    var element, name, weapon;
    
    var document = new Document();
    document.appendChild(document.createElement('Root'));

    // Tank Girl
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


    // Skeletor
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

    // Serialize
    document.serialize();
    */
}

test();
