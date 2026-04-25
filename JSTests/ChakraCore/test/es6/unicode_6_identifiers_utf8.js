//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

//Simple (<0x10000) valid character for identifier
var \u0069 = 3;
WScript.Echo(i);
WScript.Echo(\u0069);
WScript.Echo(\u{0069});
WScript.Echo(\u{00069});
WScript.Echo(\u{000069});
WScript.Echo(this.i);
WScript.Echo(this.\u0069);
WScript.Echo(this.\u{0069});
WScript.Echo(this.\u{00069});
WScript.Echo(this.\u{000069});
WScript.Echo(this["i"]);
WScript.Echo(this["\u0069"]);
WScript.Echo(this["\u{0069}"]);
WScript.Echo(this["\u{00069}"]);
WScript.Echo(this["\u{000069}"]);
WScript.Echo(eval("\u0069"));
WScript.Echo(eval("\u{0069}"));
WScript.Echo(eval("\u{00069}"));
WScript.Echo(eval("\u{000069}"));
WScript.Echo(eval("\u0069 = i + \u0069;"));
WScript.Echo(eval("\u{0069} = i + \u{0069};"));
WScript.Echo(eval("\u{00069} = i + \u{00069};"));
WScript.Echo(eval("\u{000069} = i + \u{000069};"));

//More complex variations
var 𠮷 = 1;
WScript.Echo(\u{20BB7});
WScript.Echo(\u{020BB7});
WScript.Echo(this.\u{20BB7});
WScript.Echo(this.\u{020BB7});
WScript.Echo(this["\u{20BB7}"]);
WScript.Echo(this["\u{020BB7}"]);
WScript.Echo(eval('\u{20BB7}'));
WScript.Echo(eval('\u{020BB7}'));
WScript.Echo(eval('this.\u{20BB7}'));
WScript.Echo(eval('this.\u{020BB7}'));
WScript.Echo(eval('this["\u{20BB7}"]'));
WScript.Echo(eval('this["\u{020BB7}"]'));

WScript.Echo(eval('\u{20BB7}+ \u{20BB7}'));
WScript.Echo(eval('\u{020BB7}+ \u{20BB7}'));
WScript.Echo(eval('this.\u{20BB7}+ \u{20BB7}'));
WScript.Echo(eval('this.\u{020BB7}+ \u{20BB7}'));
WScript.Echo(eval('this["\u{20BB7}"]+ \u{20BB7}'));
WScript.Echo(eval('this["\u{020BB7}"]+ \u{20BB7}'));

WScript.Echo(this["\uD842\uDFB7"]);
WScript.Echo(this["\u{D842}\uDFB7"]);
WScript.Echo(this["\uD842\u{DFB7}"]);
WScript.Echo(this["\u{D842}\u{DFB7}"]);
WScript.Echo(this["\u{0D842}\uDFB7"]);
WScript.Echo(this["\uD842\u{0DFB7}"]);
WScript.Echo(this["\u{0D842}\u{0DFB7}"]);
WScript.Echo(this["\u{00D842}\uDFB7"]);
WScript.Echo(this["\uD842\u{00DFB7}"]);
WScript.Echo(this["\u{00D842}\u{00DFB7}"]);

WScript.Echo(eval('\uD842\uDFB7'));
WScript.Echo(eval('\u{D842}\uDFB7'));
WScript.Echo(eval('\uD842\u{DFB7}'));
WScript.Echo(eval('\u{D842}\u{DFB7}'));
WScript.Echo(eval('\u{0D842}\uDFB7'));
WScript.Echo(eval('\uD842\u{0DFB7}'));
WScript.Echo(eval('\u{0D842}\u{0DFB7}'));
WScript.Echo(eval('\u{00D842}\uDFB7'));
WScript.Echo(eval('\uD842\u{00DFB7}'));
WScript.Echo(eval('\u{00D842}\u{00DFB7}'));
WScript.Echo(eval('this.\uD842\uDFB7'));
WScript.Echo(eval('this.\u{D842}\uDFB7'));
WScript.Echo(eval('this.\uD842\u{DFB7}'));
WScript.Echo(eval('this.\u{D842}\u{DFB7}'));
WScript.Echo(eval('this.\u{0D842}\uDFB7'));
WScript.Echo(eval('this.\uD842\u{0DFB7}'));
WScript.Echo(eval('this.\u{0D842}\u{0DFB7}'));
WScript.Echo(eval('this.\u{00D842}\uDFB7'));
WScript.Echo(eval('this.\uD842\u{00DFB7}'));
WScript.Echo(eval('this.\u{00D842}\u{00DFB7}'));
WScript.Echo(eval('this["\uD842\uDFB7"]'));
WScript.Echo(eval('this["\u{D842}\uDFB7"]'));
WScript.Echo(eval('this["\uD842\u{DFB7}"]'));
WScript.Echo(eval('this["\u{D842}\u{DFB7}"]'));
WScript.Echo(eval('this["\u{0D842}\uDFB7"]'));
WScript.Echo(eval('this["\uD842\u{0DFB7}"]'));
WScript.Echo(eval('this["\u{0D842}\u{0DFB7}"]'));
WScript.Echo(eval('this["\u{00D842}\uDFB7"]'));
WScript.Echo(eval('this["\uD842\u{00DFB7}"]'));
WScript.Echo(eval('this["\u{00D842}\u{00DFB7}"]'));

WScript.Echo(eval('\u{20BB7} = \uD842\uDFB7+ \u{20BB7}'));
WScript.Echo(eval('\u{20BB7} = \u{D842}\uDFB7+ \u{20BB7}'));
WScript.Echo(eval('\u{20BB7} = \uD842\u{DFB7}+ \u{20BB7}'));
WScript.Echo(eval('\u{20BB7} = \u{D842}\u{DFB7}+ \u{20BB7}'));
WScript.Echo(eval('\u{20BB7} = \u{0D842}\uDFB7+ \u{20BB7}'));
WScript.Echo(eval('\u{20BB7} = \uD842\u{0DFB7}+ \u{20BB7}'));
WScript.Echo(eval('\u{20BB7} = \u{0D842}\u{0DFB7}+ \u{20BB7}'));
WScript.Echo(eval('\u{20BB7} = \u{00D842}\uDFB7+ \u{20BB7}'));
WScript.Echo(eval('\u{20BB7} = \uD842\u{00DFB7}+ \u{20BB7}'));
WScript.Echo(eval('\u{20BB7} = \u{00D842}\u{00DFB7}+ \u{20BB7}'));
WScript.Echo(eval('\u{20BB7} = this.\uD842\uDFB7+ \u{20BB7}'));
WScript.Echo(eval('\u{20BB7} = this.\u{D842}\uDFB7+ \u{20BB7}'));
WScript.Echo(eval('\u{20BB7} = this.\uD842\u{DFB7}+ \u{20BB7}'));
WScript.Echo(eval('\u{20BB7} = this.\u{D842}\u{DFB7}+ \u{20BB7}'));
WScript.Echo(eval('\u{20BB7} = this.\u{0D842}\uDFB7+ \u{20BB7}'));
WScript.Echo(eval('\u{20BB7} = this.\uD842\u{0DFB7}+ \u{20BB7}'));
WScript.Echo(eval('\u{20BB7} = this.\u{0D842}\u{0DFB7}+ \u{20BB7}'));
WScript.Echo(eval('\u{20BB7} = this.\u{00D842}\uDFB7+ \u{20BB7}'));
WScript.Echo(eval('\u{20BB7} = this.\uD842\u{00DFB7}+ \u{20BB7}'));
WScript.Echo(eval('\u{20BB7} = this.\u{00D842}\u{00DFB7}+ \u{20BB7}'));
WScript.Echo(eval('\u{20BB7} = this["\uD842\uDFB7"]+ \u{20BB7}'));
WScript.Echo(eval('\u{20BB7} = this["\u{D842}\uDFB7"]+ \u{20BB7}'));
WScript.Echo(eval('\u{20BB7} = this["\uD842\u{DFB7}"]+ \u{20BB7}'));
WScript.Echo(eval('\u{20BB7} = this["\u{D842}\u{DFB7}"]+ \u{20BB7}'));
WScript.Echo(eval('\u{20BB7} = this["\u{0D842}\uDFB7"]+ \u{20BB7}'));
WScript.Echo(eval('\u{20BB7} = this["\uD842\u{0DFB7}"]+ \u{20BB7}'));
WScript.Echo(eval('\u{20BB7} = this["\u{0D842}\u{0DFB7}"]+ \u{20BB7}'));
WScript.Echo(eval('\u{20BB7} = this["\u{00D842}\uDFB7"]+ \u{20BB7}'));
WScript.Echo(eval('\u{20BB7} = this["\uD842\u{00DFB7}"]+ \u{20BB7}'));
WScript.Echo(eval('\u{20BB7} = this["\u{00D842}\u{00DFB7}"]+ \u{20BB7}'));

try{
        eval("FOR4 : for(var i=1;i<2;i++){FOR4NESTED : for(var j=1;j<2;j++) { continue\u2029FOR4; } while(0);}");
        if (j!==2) {
                $ERROR('#4: Since LineTerminator(U-2029) between continue and Identifier not allowed continue evaluates without label');
        }
} catch(e){
        $ERROR('#4.1: eval("FOR4 : for(var i=1;i<2;i++){FOR4NESTED : for(var j=1;j<2;j++) { continue\\u2029FOR4; } while(0);}"); does not lead to throwing exception');
}


//Some interesting cases
var _\u{20BB7} = 'a';
eval("WScript.Echo(_\u{20BB7})");
var _\u0524 = 'a';
eval("WScript.Echo(_\u0524)");
var $\u{20BB7} = 'b';
eval("WScript.Echo($\u{20BB7})");
var $\u0524 = 'b';
eval("WScript.Echo($\u0524)");

var $00xxx\u0069\u0524\u{20BB7} = 'c';
WScript.Echo(eval('$00xxxi\u0524\u{D842}\uDFB7'));

//These 2 are valid
//  WScript.Echo(\u2e2f);
//  WScript.Echo(_\u2e2f);


var ℘abc = null;
// This one should throw
var \u4e3d = 5;
