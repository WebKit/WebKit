/*
 * Copyright (C) 2010, 2011, 2015 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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

var RedBlackTree = (function(){
    function compare(a, b) {
        return a.compareTo(b);
    }
    
    function treeMinimum(x) {
        while (x.left)
            x = x.left;
        return x;
    }
    
    function treeMaximum(x) {
        while (x.right)
            x = x.right;
        return x;
    }
    
    function Node(key, value) {
        this.key = key;
        this.value = value;
        this.left = null;
        this.right = null;
        this.parent = null;
        this.color = "red";
    }
    
    Node.prototype.successor = function() {
        var x = this;
        if (x.right)
            return treeMinimum(x.right);
        var y = x.parent;
        while (y && x == y.right) {
            x = y;
            y = y.parent;
        }
        return y;
    };
    
    Node.prototype.predecessor = function() {
        var x = this;
        if (x.left)
            return treeMaximum(x.left);
        var y = x.parent;
        while (y && x == y.left) {
            x = y;
            y = y.parent;
        }
        return y;
    };
    
    Node.prototype.toString = function() {
        return this.key + "=>" + this.value + " (" + this.color + ")";
    };
    
    function RedBlackTree() {
        this._root = null;
    }
    
    RedBlackTree.prototype.put = function(key, value) {
        var insertionResult = this._treeInsert(key, value);
        if (!insertionResult.isNewEntry)
            return insertionResult.oldValue;
        var x = insertionResult.newNode;
        
        while (x != this._root && x.parent.color == "red") {
            if (x.parent == x.parent.parent.left) {
                var y = x.parent.parent.right;
                if (y && y.color == "red") {
                    // Case 1
                    x.parent.color = "black";
                    y.color = "black";
                    x.parent.parent.color = "red";
                    x = x.parent.parent;
                } else {
                    if (x == x.parent.right) {
                        // Case 2
                        x = x.parent;
                        this._leftRotate(x);
                    }
                    // Case 3
                    x.parent.color = "black";
                    x.parent.parent.color = "red";
                    this._rightRotate(x.parent.parent);
                }
            } else {
                // Same as "then" clause with "right" and "left" exchanged.
                var y = x.parent.parent.left;
                if (y && y.color == "red") {
                    // Case 1
                    x.parent.color = "black";
                    y.color = "black";
                    x.parent.parent.color = "red";
                    x = x.parent.parent;
                } else {
                    if (x == x.parent.left) {
                        // Case 2
                        x = x.parent;
                        this._rightRotate(x);
                    }
                    // Case 3
                    x.parent.color = "black";
                    x.parent.parent.color = "red";
                    this._leftRotate(x.parent.parent);
                }
            }
        }
        
        this._root.color = "black";
        return null;
    };
    
    RedBlackTree.prototype.remove = function(key) {
        var z = this._findNode(key);
        if (!z)
            return null;
        
        // Y is the node to be unlinked from the tree.
        var y;
        if (!z.left || !z.right)
            y = z;
        else
            y = z.successor();

        // Y is guaranteed to be non-null at this point.
        var x;
        if (y.left)
            x = y.left;
        else
            x = y.right;
        
        // X is the child of y which might potentially replace y in the tree. X might be null at
        // this point.
        var xParent;
        if (x) {
            x.parent = y.parent;
            xParent = x.parent;
        } else
            xParent = y.parent;
        if (!y.parent)
            this._root = x;
        else {
            if (y == y.parent.left)
                y.parent.left = x;
            else
                y.parent.right = x;
        }
        
        if (y != z) {
            if (y.color == "black")
                this._removeFixup(x, xParent);
            
            y.parent = z.parent;
            y.color = z.color;
            y.left = z.left;
            y.right = z.right;
            
            if (z.left)
                z.left.parent = y;
            if (z.right)
                z.right.parent = y;
            if (z.parent) {
                if (z.parent.left == z)
                    z.parent.left = y;
                else
                    z.parent.right = y;
            } else
                this._root = y;
        } else if (y.color == "black")
            this._removeFixup(x, xParent);
        
        return z.value;
    };
    
    RedBlackTree.prototype.get = function(key) {
        var node = this._findNode(key);
        if (!node)
            return null;
        return node.value;
    };
    
    RedBlackTree.prototype.forEach = function(callback) {
        if (!this._root)
            return;
        for (var current = treeMinimum(this._root); current; current = current.successor())
            callback(current.key, current.value);
    };
    
    RedBlackTree.prototype.asArray = function() {
        var result = [];
        this.forEach(function(key, value) {
            result.push({key: key, value: value});
        });
        return result;
    };
    
    RedBlackTree.prototype.toString = function() {
        var result = "[";
        var first = true;
        this.forEach(function(key, value) {
            if (first)
                first = false;
            else
                result += ", ";
            result += key + "=>" + value;
        });
        return result + "]";
    };
    
    RedBlackTree.prototype._findNode = function(key) {
        for (var current = this._root; current;) {
            var comparisonResult = compare(key, current.key);
            if (!comparisonResult)
                return current;
            if (comparisonResult < 0)
                current = current.left;
            else
                current = current.right;
        }
        return null;
    };
    
    RedBlackTree.prototype._treeInsert = function(key, value) {
        var y = null;
        var x = this._root;
        while (x) {
            y = x;
            var comparisonResult = key.compareTo(x.key);
            if (comparisonResult < 0)
                x = x.left;
            else if (comparisonResult > 0)
                x = x.right;
            else {
                var oldValue = x.value;
                x.value = value;
                return {isNewEntry:false, oldValue:oldValue};
            }
        }
        var z = new Node(key, value);
        z.parent = y;
        if (!y)
            this._root = z;
        else {
            if (key.compareTo(y.key) < 0)
                y.left = z;
            else
                y.right = z;
        }
        return {isNewEntry:true, newNode:z};
    };
    
    RedBlackTree.prototype._leftRotate = function(x) {
        var y = x.right;
        
        // Turn y's left subtree into x's right subtree.
        x.right = y.left;
        if (y.left)
            y.left.parent = x;
        
        // Link x's parent to y.
        y.parent = x.parent;
        if (!x.parent)
            this._root = y;
        else {
            if (x == x.parent.left)
                x.parent.left = y;
            else
                x.parent.right = y;
        }
        
        // Put x on y's left.
        y.left = x;
        x.parent = y;
        
        return y;
    };
    
    RedBlackTree.prototype._rightRotate = function(y) {
        var x = y.left;
        
        // Turn x's right subtree into y's left subtree.
        y.left = x.right;
        if (x.right)
            x.right.parent = y;
        
        // Link y's parent to x;
        x.parent = y.parent;
        if (!y.parent)
            this._root = x;
        else {
            if (y == y.parent.left)
                y.parent.left = x;
            else
                y.parent.right = x;
        }
        
        x.right = y;
        y.parent = x;
        
        return x;
    };
    
    RedBlackTree.prototype._removeFixup = function(x, xParent) {
        while (x != this._root && (!x || x.color == "black")) {
            if (x == xParent.left) {
                // Note: the text points out that w cannot be null. The reason is not obvious from
                // simply looking at the code; it comes about from the properties of the red-black
                // tree.
                var w = xParent.right;
                if (w.color == "red") {
                    // Case 1
                    w.color = "black";
                    xParent.color = "red";
                    this._leftRotate(xParent);
                    w = xParent.right;
                }
                if ((!w.left || w.left.color == "black")
                    && (!w.right || w.right.color == "black")) {
                    // Case 2
                    w.color = "red";
                    x = xParent;
                    xParent = x.parent;
                } else {
                    if (!w.right || w.right.color == "black") {
                        // Case 3
                        w.left.color = "black";
                        w.color = "red";
                        this._rightRotate(w);
                        w = xParent.right;
                    }
                    // Case 4
                    w.color = xParent.color;
                    xParent.color = "black";
                    if (w.right)
                        w.right.color = "black";
                    this._leftRotate(xParent);
                    x = this._root;
                    xParent = x.parent;
                }
            } else {
                // Same as "then" clause with "right" and "left" exchanged.
                
                var w = xParent.left;
                if (w.color == "red") {
                    // Case 1
                    w.color = "black";
                    xParent.color = "red";
                    this._rightRotate(xParent);
                    w = xParent.left;
                }
                if ((!w.right || w.right.color == "black")
                    && (!w.left || w.left.color == "black")) {
                    // Case 2
                    w.color = "red";
                    x = xParent;
                    xParent = x.parent;
                } else {
                    if (!w.left || w.left.color == "black") {
                        // Case 3
                        w.right.color = "black";
                        w.color = "red";
                        this._leftRotate(w);
                        w = xParent.left;
                    }
                    // Case 4
                    w.color = xParent.color;
                    xParent.color = "black";
                    if (w.left)
                        w.left.color = "black";
                    this._rightRotate(xParent);
                    x = this._root;
                    xParent = x.parent;
                }
            }
        }
        if (x)
            x.color = "black";
    };
    
    return RedBlackTree;
})();

