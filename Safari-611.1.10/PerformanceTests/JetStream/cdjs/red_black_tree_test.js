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

load("red_black_tree.js");

// The control string is a list of commands. Each command is two characters, with the first
// identifying the operation and the second giving a key.
var test = (function(){
    function PairVector() {
        this._vector = [];
    }

    PairVector.prototype.put = function(key, value) {
        for (var i = 0; i < this._vector.length; ++i) {
            if (!this._vector[i].key.compareTo(key)) {
                var result = this._vector[i].value;
                this._vector[i].value = value;
                return result;
            }
        }
        this._vector.push({key: key, value: value});
        return null;
    };

    PairVector.prototype.remove = function(key, value) {
        for (var i = 0; i < this._vector.length; ++i) {
            if (!this._vector[i].key.compareTo(key)) {
                var result = this._vector[i].value;
                this._vector[i] = this._vector[this._vector.length - 1];
                this._vector.pop();
                return result;
            }
        }
        return null;
    };

    PairVector.prototype.get = function(key, value) {
        for (var i = 0; i < this._vector.length; ++i) {
            if (!this._vector[i].key.compareTo(key))
                return this._vector[i].value;
        }
        return null;
    };
    
    PairVector.prototype.toString = function() {
        var copy = this._vector.concat();
        copy.sort(function(a, b) {
            return a.key.compareTo(b.key);
        });
        var result = "[";
        for (var i = 0; i < copy.length; ++i) {
            if (i)
                result += ", ";
            result += copy[i].key + "=>" + copy[i].value;
        }
        return result + "]";
    };
    
    String.prototype.compareTo = String.prototype.localeCompare;

    var counter = 0;
    
    return function(controlString) {
        print("Running " + JSON.stringify(controlString) + ":");
        var asVector = new PairVector();
        var asTree = new RedBlackTree();
        
        function fail(when) {
            throw new Error(
                "FAIL: " + when + ", tree = " + asTree + ", vector = " + asVector);
        }
        
        for (var index = 0; index < controlString.length; index += 2) {
            var command = controlString[index];
            var key = controlString[index + 1];
            var value = ++counter;
            
            switch (command) {
            case "+":
                var oldVectorValue = asVector.put(key, value);
                var oldTreeValue = asTree.put(key, value);
                if (oldVectorValue !== oldTreeValue) {
                    fail("Adding " + key + "=>" + value + ", vector got " +
                         oldVectorValue + " but tree got " + oldTreeValue);
                }
                break;
                
            case "*":
                var oldVectorValue = asVector.get(key);
                var oldTreeValue = asTree.get(key);
                if (oldVectorValue !== oldTreeValue) {
                    fail("Getting " + key + ", vector got " +
                         oldVectorValue + " but tree got " + oldTreeValue);
                }
                break;
                
            case "!":
                var oldVectorValue = asVector.remove(key);
                var oldTreeValue = asTree.remove(key);
                if (oldVectorValue !== oldTreeValue) {
                    fail("Removing " + key + ", vector got " +
                         oldVectorValue + " but tree got " + oldTreeValue);
                }
                break;
                
            default:
                throw Error("Bad command: " + command);
            }
        }
        
        if ("" + asVector != "" + asTree)
            fail("Bad result at end");
        
        print("    Success, tree is: " + asTree);
    };
})();

test("");
test("+a");
test("*x!z");
test("+a*x!z");
test("+a*a!a");
test("+a+b");
test("+a+b+c");
test("+a+b+c+d");
test("+a+b+c+d+a+b+c+d+a+b+c+d+a+b+c+d+a+b+c+d+a+b+c+d+a+b+c+d+a+b+c+d+a+b+c+d+a+b+c+d+a+b+c+d+a+b+c+d");
test("+a+b+c+d+a+b+c+d+a+b+c+d+a+b+c+d+a+b+c+d+a+b+c+d+a+b+c+d+a+b+c+d+a+b+c+d+a+b+c+d+a+b+c+d+a+b+c+d+e+f+g+h+i+j+k+l+m+n+o+p+q+r+s+t+u+v+x+y+z");
test("+a+b+c+d+a+b+c+d+a+b+c+d+a+b+c+d+a+b+c+d+a+b+c+d+a+b+c+d+a+b+c+d+a+b+c+d+a+b+c+d+a+b+c+d+a+b+c+d+e+f+g+h+i+j+k+l+m+n+o+p+q+r+s+t+u+v+x+y+z*a*b*c*d*e*f*g*h*i*j*k*l*m*n*o*p*q*r*s*t*u*v*w*x*y*z!a!b!c!d!e!f!g!h!i!j!k!l!m!n!o!p!q!r!s!t!u!v!w!x!y!z");

