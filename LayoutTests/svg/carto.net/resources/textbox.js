/*
Scripts to create interactive textboxes in SVG using ECMA script
Copyright (C) <2006>  <Andreas Neumann>
Version 1.1.2, 2006-10-04
neumann@karto.baug.ethz.ch
http://www.carto.net/
http://www.carto.net/neumann/

Credits:
* Initial code was taken from Olaf Schnabel --> schnabel@karto.baug.ethz.ch (thanks!)
* Thanks also to many people of svgdevelopers@yahoogroups.com
* bug report and fix from Volker Gersabeck (make textbox namespace aware and corrected .setValue method when text was transformed)
* bug report and fix from David Boyd (pressing delete key in ASV would fail if cursor is at end of textbox)
* enhancement suggestion and bug report by Michael Mehldorn (callback function was called twice in case of enter key, accept also integer values in method .setValue())

----

Documentation: http://www.carto.net/papers/svg/gui/textbox/

----

current version: 1.1.2

version history:
1.0 (2006-05-03)
initial version

1.0.1 (2006-05-04)
fixed a bug with the delete key after clicking into the textbox
fixed a bug with removing text selection when clicking again after the selectionbox was visible

1.02 (2006-05-18):
made object multi-namespace aware (hopefully), this probably needs more testing
it is now allowed to pass numbers in the constructor and .setValue() method

1.03 (2006-05-22):
fixed a bug in internal method .testSupportsChar() when using an initially empty textbox

1.04 (2006-06-22)
added constructor parameter this.parentNode; this.parentNode can be of type String (id) or a node reference (g or svg element)
replaced this.textboxGroup with this.parentGroup to be compatible with other GUI elements; fixed an "out of index" bug that occasionally appeared in Opera 9 when calculating the cursor position

1.1 (2006-07-11)
fixed a bug with the delete key (ASV only) if cursor was at the end (thanks to David Boyd), fixed another bug with delete key if cursor was at the end it accidentally deleted the first character, fixed a bug in the method .setValue() (thanks to Volker Gersabeck)
added constructor parameter textYOffset, added methods ".moveTo(x,y)" and ".resize(width)"

1.1.1 (2006-07-13)
changed the internal structure a bit. An additional group element is now created. This allows several textboxes to be placed in the same parent group. Previously this had failed. No changes in constructor parameters and methods in this release.

1.1.2 (2006-10-04)
added parameter "fireFunction" to the "setValue()" method in order to determine whether the associated callback function should be fired or not
introduced new "changetype" with value "set" which indicates that the textbox value was set by method "setValue"

-------

This ECMA script library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library (lesser_gpl.txt); if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

----

original document site: http://www.carto.net/papers/svg/gui/textbox/
Please contact the author in case you want to use code or ideas commercially.
If you use this code, please include this copyright header, the included full
LGPL 2.1 text and read the terms provided in the LGPL 2.1 license
(http://www.gnu.org/copyleft/lesser.txt)

-------------------------------

Please report bugs and send improvements to neumann@karto.baug.ethz.ch
If you use this control, please link to the original (http://www.carto.net/papers/svg/gui/textbox/)
somewhere in the source-code-comment or the "about" of your project and give credits, thanks!

*/

function textbox(id,parentNode,defaultVal,maxChars,x,y,boxWidth,boxHeight,textYOffset,textStyles,boxStyles,cursorStyles,selBoxStyles,allowedChars,functionToCall) {
    var nrArguments = 15;
    var createTextBox= true;
    if (arguments.length == nrArguments) {
        this.id = id; //the id of the textbox
        this.parentNode = parentNode;  //can be of type string (id) or node reference (svg or g node)
        this.maxChars = maxChars; //maximum characters allowed
        this.defaultVal = defaultVal.toString(); //default value to be filled in when textbox is created
        this.x = x; //left of background rectangle
        this.y = y; //top of background rectangle
        this.boxWidth = boxWidth; //background rectangle width
        this.boxHeight = boxHeight; //background rectangle height
        this.textYOffset = textYOffset; //the offset of the text element in relation to the upper side of the textbox rectangle
        this.textStyles = textStyles; //array containing text attributes
        if (!this.textStyles["font-size"]) {
            this.textStyles["font-size"] = 15;
        }
        this.boxStyles = boxStyles; //array containing box styles attributes
        this.cursorStyles = cursorStyles; //array containing text attributes
        this.selBoxStyles = selBoxStyles; //array containing box styles attributes
        //allowedChars contains regular expressions of allowed character ranges
        if (allowedChars) {
            if (typeof(allowedChars) == "string") {
                if (allowedChars.length > 0) {
                    this.RegExp = new RegExp(allowedChars);
                }
            }
        }
        else {
            this.RegExp = undefined;
        }
        this.functionToCall = functionToCall; //function to be called if textbox looses focus or enter key is pressed
        this.textboxRect = null; //later holds reference to rect element
        this.textboxText = null; //later holds reference to text element
        this.textboxTextContent = null; //later holds reference to content of text element (first child)
        this.textboxCursor = null; //later holds reference to cursor
        this.textboxStatus = 0; //status 0 means unitialized, 1 means partially initalized, 2 means fully initialized and ready to remove event listeners again, 5 means new value was set by method .setValue()
        this.cursorPosition = 0; //position in whole string
        this.transX = 0; //offset on the left if text string is larger than box
        this.textVal = this.defaultVal; //this is the current text string of the content
        this.shiftDown = false; //boolean value that says if shift was pressed
        this.mouseDown = false; //boolean value that says if mousedown is active
        this.startSelection = 0; //position of the start of the selection
        this.startOrigSelection = 0; //original start position of selection
        this.endSelection = 0; //position of the end of the selection
        this.selectionRectVisible = false; //indicates if selection rect is visible or not
        this.svg = null; //later a nested svg that does clipping
        this.supportsCharGeom = true; //defines if viewer supports geometry calculations on individual characters, such as .getCharAtPosition(SVGPoint)
    }
    else {
        createTextBox = false;
        alert("Error ("+id+"): wrong nr of arguments! You have to pass over "+nrArguments+" parameters.");
    }
    if (createTextBox) {
        this.timer = new Timer(this); //a Timer instance for calling the functionToCall
        this.timerMs = 200; //a constant of this object that is used in conjunction with the timer - functionToCall is called after 200 ms
        this.createTextbox(); //method to initialize textbox
    }
    else {
        alert("Could not create textbox with id '"+id+"' due to errors in the constructor parameters");        
    }
}

//create textbox
textbox.prototype.createTextbox = function() {
    var result = this.testParent();
    if (result) {
        //create a textbox parent group
        this.textboxParent = document.createElementNS(svgNS,"g");
        this.parentGroup.appendChild(this.textboxParent);
        
        //create background rect
        this.textboxRect = document.createElementNS(svgNS,"rect");
        this.textboxRect.setAttributeNS(null,"x",this.x);
        this.textboxRect.setAttributeNS(null,"y",this.y);
        this.textboxRect.setAttributeNS(null,"width",this.boxWidth);
        this.textboxRect.setAttributeNS(null,"height",this.boxHeight);
        this.textboxRect.setAttributeNS(null,"pointer-events","fill");
        for (var attrib in this.boxStyles) {
            this.textboxRect.setAttributeNS(null,attrib,this.boxStyles[attrib]);
        }
        this.textboxParent.appendChild(this.textboxRect);
        
        this.svg = document.createElementNS(svgNS,"svg");
        this.svg.setAttributeNS(null,"x",this.x + this.textStyles["font-size"] / 4);
        this.svg.setAttributeNS(null,"y",this.y + this.boxHeight * 0.02);
        this.svg.setAttributeNS(null,"width",this.boxWidth - (this.textStyles["font-size"]) / 2);
        this.svg.setAttributeNS(null,"height",this.boxHeight * 0.96);
        this.svg.setAttributeNS(null,"viewBox",(this.x + this.textStyles["font-size"] / 4)+" "+(this.y + this.boxHeight * 0.02)+" "+(this.boxWidth - (this.textStyles["font-size"]) / 2)+" "+(this.boxHeight * 0.96));
        this.textboxParent.appendChild(this.svg);
        
        //create group to hold text, selectionRect and cursor
        this.textboxTextGroup = document.createElementNS(svgNS,"g");
        this.svg.appendChild(this.textboxTextGroup);
        
        //create text element
        this.textboxText = document.createElementNS(svgNS,"text");
        this.textboxText.setAttributeNS(null,"x",(this.x + this.textStyles["font-size"] / 3));
        this.textboxText.setAttributeNS(null,"y",(this.y + this.textYOffset));
        for (var attrib in this.textStyles) {
            value = this.textStyles[attrib];
            if (attrib == "font-size") {
                value += "px";
            }
            this.textboxText.setAttributeNS(null,attrib,value);
        }
        this.textboxText.setAttributeNS(null,"id",this.id+"Text");
        if (myMapApp.navigator != "Opera") {
            this.textboxText.setAttributeNS(null,"pointer-events","none");
        }
        this.textboxText.setAttributeNS("http://www.w3.org/XML/1998/namespace","space","preserve");
        //check if defaultVal is longer than maxChars and truncate if necessary
        if (this.defaultVal.length <= this.maxChars) {
            this.textboxTextContent = document.createTextNode(this.defaultVal);
            this.cursorPosition = this.defaultVal.length - 1;
        }
        else {
            alert("the default textbox value is longer than the maximum of allowed characters\nDefault val will be truncated.");
            this.textVal = this.defaultVal.substr(0,(this.maxChars - 1));
            this.textboxTextContent = document.createTextNode(this.textVal);
            this.cursorPosition = this.maxChars - 1;
        }
        this.textboxText.appendChild(this.textboxTextContent);
        this.textboxTextGroup.appendChild(this.textboxText);
        
        //create selection rectangle
        this.selectionRect = document.createElementNS(svgNS,"rect");
        this.selectionRect.setAttributeNS(null,"x",(this.x + this.textStyles["font-size"] / 3));
        this.selectionRect.setAttributeNS(null,"y",(this.y + this.textYOffset - this.textStyles["font-size"] * 0.9));
        this.selectionRect.setAttributeNS(null,"width",(this.textStyles["font-size"] * 2));
        this.selectionRect.setAttributeNS(null,"height",this.textStyles["font-size"] * 1.1);
        for (var attrib in this.selBoxStyles) {
            this.selectionRect.setAttributeNS(null,attrib,this.selBoxStyles[attrib]);
        }
        this.selectionRect.setAttributeNS(null,"display","none");
        this.textboxTextGroup.appendChild(this.selectionRect);
            
        //create cursor element
        this.textboxCursor = document.createElementNS(svgNS,"line");
        this.textboxCursor.setAttributeNS(null,"x1",this.x);
        this.textboxCursor.setAttributeNS(null,"y1",(this.y + this.textYOffset + this.textStyles["font-size"] * 0.2));
        this.textboxCursor.setAttributeNS(null,"x2",this.x);
        this.textboxCursor.setAttributeNS(null,"y2",(this.y + this.textYOffset - this.textStyles["font-size"] * 0.9));
        for (var attrib in this.cursorStyles) {
            this.textboxCursor.setAttributeNS(null,attrib,this.cursorStyles[attrib]);
        }
        this.textboxCursor.setAttributeNS(null,"id",this.id+"Cursor");
        this.textboxCursor.setAttributeNS(null,"visibility","hidden");
        this.textboxTextGroup.appendChild(this.textboxCursor);
        
        // add event listeners to the textbox group
        this.textboxParent.addEventListener("mousedown",this,false);
        this.textboxParent.addEventListener("mousemove",this,false);
        this.textboxParent.addEventListener("mouseup",this,false);
        this.textboxParent.setAttributeNS(null,"cursor","text");
        
        //test if the svgviewer supports getting geometries of individual characters
        this.timer.setTimeout("testSupportsChar",this.timerMs);
    }
    else {
        alert("could not create or reference 'parentNode' of textbox with id '"+this.id+"'");
    }
}

textbox.prototype.testSupportsChar = function() {
    //determine whether viewer is capable of charGeom functions
    var isEmpty = false;
    //temporarily create a space to test if getStartPosition is available
    if (this.textVal.length == 0) {
        isEmpty = true;
        this.textboxTextContent.nodeValue = " ";
    }        
    try {
        var dummy = this.textboxText.getStartPositionOfChar(0).x;
    }
    catch(er) {
        this.supportsCharGeom = false;
    }
    if (isEmpty) {
        this.textboxTextContent.nodeValue = "";
    }
}

//test if window group exists or create a new group at the end of the file
textbox.prototype.testParent = function() {
    //test if of type object
    var nodeValid = false;
    if (typeof(this.parentNode) == "object") {
        if (this.parentNode.nodeName == "svg" || this.parentNode.nodeName == "g" || this.parentNode.nodeName == "svg:svg" || this.parentNode.nodeName == "svg:g") {
            this.parentGroup = this.parentNode;
            nodeValid = true;
        }
    }
    else if (typeof(this.parentNode) == "string") { 
        //first test if textbox group exists
        if (!document.getElementById(this.parentNode)) {
            this.parentGroup = document.createElementNS(svgNS,"g");
            this.parentGroup.setAttributeNS(null,"id",this.parentNode);
            document.documentElement.appendChild(this.parentGroup);
            nodeValid = true;
           }
           else {
               this.parentGroup = document.getElementById(this.parentNode);
               nodeValid = true;
           }
       }
       return nodeValid;
}

//remove all textbox elements
textbox.prototype.removeTextbox = function() {
    this.parentGroup.removeChild(this.textboxParent);
}

//event handler functions
textbox.prototype.handleEvent = function(evt) {
    if (evt.type == "mousedown") {
        //this case is when the user mousedowns outside the textbox; in this case the textbox should behave like the user
        //pressed the enter key
        if ((evt.currentTarget.nodeName == "svg" || evt.currentTarget.nodeName == "svg:svg") && this.textboxStatus == 2) {
            this.release();
        }
        else {
            //this is for preparing the textbox with first mousedown and to reposition cursor with each subsequent mousedowns
            if (evt.currentTarget.nodeName == "g" || evt.currentTarget.nodeName == "svg:g") {
                this.calcCursorPosFromMouseEvt(evt);
                // set event listeners, this is only done on first mousedown in the textbox
                if (this.textboxStatus == 0) {
                    if (myMapApp.navigator == "Adobe") {
                        document.documentElement.addEventListener("keydown",this,false);
                    }
                    document.documentElement.addEventListener("keypress",this,false);
                    document.documentElement.addEventListener("mousedown",this,false);
                    document.documentElement.addEventListener("mouseup",this,false);
                    document.documentElement.addEventListener("mousemove",this,false);
                    // set textbox status
                    this.textboxStatus = 1;
                    // set cursor visibility
                    this.textboxCursor.setAttributeNS(null,"visibility","visible");
                }
                else {
                    evt.stopPropagation();
                }
                this.setCursorPos();
                //start text selection
                this.startOrigSelection = this.cursorPosition + 1;
                this.startSelection = this.cursorPosition + 1;
                this.endSelection = this.cursorPosition + 2;
                //remove previous selections
                this.selectionRect.setAttributeNS(null,"display","none");
                this.selectionRectVisible = false;
                //set status of shiftDown and mouseDown
                this.shiftDown = true;
                this.mouseDown = true;
            }
            //this mouseup should be received from background rectangle (received via document element)
            else {
                this.textboxStatus = 2;
            }
        }
    }
    if (evt.type == "mousemove") {
        if (this.textboxStatus == 2 && this.shiftDown && this.mouseDown && this.supportsCharGeom) {
                this.calcCursorPosFromMouseEvt(evt);
                this.setCursorPos();
                if (this.cursorPosition + 1 != this.startOrigSelection) {
                    if (this.cursorPosition + 1 < this.startOrigSelection) {
                        this.endSelection = this.startOrigSelection;
                        this.startSelection = this.cursorPosition + 1;                
                    }
                    else {
                        this.startSelection = this.startOrigSelection;
                        this.endSelection = this.cursorPosition + 1;                
                    }
                    this.selectionRect.setAttributeNS(null,"display","inherit");
                    this.selectionRectVisible = true;
                    var rectX = this.textboxText.getStartPositionOfChar(this.startSelection).x
                    this.selectionRect.setAttributeNS(null,"x",rectX);
                    this.selectionRect.setAttributeNS(null,"width",(this.textboxText.getEndPositionOfChar(this.endSelection - 1).x - rectX));
                    var cursorX = parseInt(this.textboxCursor.getAttributeNS(null,"x1"));
                    //if cursor runs out on the right, scroll to the right
                    if ((cursorX + this.transX) > (this.x + this.boxWidth - this.textStyles["font-size"] / 3)) {
                        this.transX = (this.x + this.boxWidth - this.textStyles["font-size"] / 3) - cursorX;
                        this.textboxTextGroup.setAttributeNS(null,"transform","translate("+this.transX+",0)");
                    }
                    //if cursor runs out on the left, scroll to the left
                    if ((cursorX + this.transX) < (this.x + this.textStyles["font-size"] / 3)) {
                        this.transX += (this.x + this.textStyles["font-size"] / 3) - (cursorX + this.transX);
                        if (this.transX * -1 < (this.boxWidth - this.textStyles["font-size"])) {
                            this.transX = 0;
                        }
                        this.textboxTextGroup.setAttributeNS(null,"transform","translate("+this.transX+",0)");
                    }
                }
        }
    }
    if (evt.type == "mouseup") {
        if (this.textboxStatus == 2 && this.shiftDown && this.mouseDown) {
                this.mouseDown = false;
        }
    }
    if (evt.type == "keypress") {            
        if (evt.keyCode) {
            var charCode = evt.keyCode;
        }
        else {
            var charCode = evt.charCode;
        }
        var keyCode = parseInt(charCode);
        var charCode = undefined;
        this.changed = false; //this tracks if the text was actually changed (if the content was changed)
        //alert("keyCode="+evt.keyCode+", charCode="+evt.charCode+", shiftKey"+evt.shiftKey);
                
        if (myMapApp.navigator != "Adobe") {
            //note that Adobe SVG enters this method through the keydown event
            this.specialCharacters(evt);
        }
        
        if (myMapApp.navigator == "Opera") {
            if (evt.keyCode > 31 && evt.keyCode != 35 && evt.keyCode != 36 && evt.keyCode != 37 && evt.keyCode != 39 && evt.keyCode != 46) {
                evt.charCode = evt.keyCode;
            }
        }
        
        //all real characters
        if (keyCode > 31 && keyCode != 127 && keyCode < 65535 && evt.charCode && evt.charCode < 65535) {            
            var textChanged = false;
            var keychar = String.fromCharCode(keyCode);
            var result = 0;
            if (this.RegExp) {
                result = keychar.search(this.RegExp);
            }            
            if (result == 0) {
                if (this.shiftDown && this.selectionRectVisible) {
                    var tempText = this.textVal.substring(0,this.startSelection) + keychar + this.textVal.substring(this.endSelection,this.textVal.length);
                    this.textVal = tempText;
                    this.cursorPosition = this.startSelection - 1;
                    textChanged = true;
                    this.releaseShift();
                }
                else if (this.textVal.length < this.maxChars) {
                    if (this.cursorPosition == this.textVal.length -1) {
                        this.textVal += keychar; // append new input character
                    }
                    else {
                        var tempText = this.textVal.substring(0,(this.cursorPosition + 1)) + keychar + this.textVal.substring((this.cursorPosition + 1),(this.textVal.length));
                        this.textVal = tempText;
                    }
                    textChanged = true;
                }
                if (this.textVal.length < this.maxChars) {
                    this.cursorPosition++;
                }
                else {
                    if (textChanged) {
                        if (this.cursorPosition < this.textVal.length) {
                            this.cursorPosition++;    
                        }
                        else {
                            this.cursorPosition = this.textVal.length - 1;
                        }
                    }    
                }
                //make sure that the selections and shift key are resetted
                this.startSelection = this.cursorPosition;
                this.endSelection = this.cursorPosition;
                this.shiftDown = false;
                if (textChanged) {
                    this.textboxTextContent.nodeValue=this.textVal;
                    this.changed = true;
                    //update cursor position
                    this.setCursorPos();
                    var cursorX = parseInt(this.textboxCursor.getAttributeNS(null,"x1"));
                    if ((cursorX + this.transX) > (this.x + this.boxWidth - this.textStyles["font-size"] / 3)) {
                        this.transX = (this.x + this.boxWidth - this.textStyles["font-size"] / 3) - (cursorX + this.transX) + this.transX;
                        this.textboxTextGroup.setAttributeNS(null,"transform","translate("+this.transX+",0)");
                    }
                }
            }
        }
        //fire function if text changed
        if (this.changed) {
            this.timer.setTimeout("fireFunction",this.timerMs);
        }
        //suppress unwanted browser shortcuts. e.g. in Opera or Mozilla
        evt.preventDefault();
    }
    //this part is only because the Adobe viewer doesn't return certain keys "onkeypress"
    if (evt.type == "keydown") {
        this.specialCharacters(evt);
    }
}

textbox.prototype.specialCharacters = function(evt) {
        if (evt.keyCode) {
            var charCode = evt.keyCode;
        }
        else {
            var charCode = evt.charCode;
        }
        var keyCode = parseInt(charCode);
        var charCode = undefined;
        
        //backspace key
        if (keyCode == 8) {
            //only do it if there is still text and cursor is not at start position
            if (this.textVal.length > 0 && this.cursorPosition > -2) {
                //first deal with text content, delete chars according to cursor position
                if (this.shiftDown && this.selectionRectVisible) {
                    var tempText = this.textVal.substring(0,this.startSelection) + this.textVal.substring(this.endSelection,this.textVal.length);
                    this.textVal = tempText;
                    this.cursorPosition = this.startSelection - 1;
                    this.releaseShift();
                }
                else { 
                    if (this.cursorPosition == this.textVal.length - 1) {
                        //cursor is at the end of textVal
                        this.textVal=this.textVal.substring(0,this.textVal.length-1);
                    }
                    else {
                        //cursor is in between
                        var tempText = this.textVal.substring(0,(this.cursorPosition)) + this.textVal.substring((this.cursorPosition + 1),(this.textVal.length));
                        this.textVal = tempText;
                    }
                    //decrease cursor position
                    if (this.cursorPosition > -1) {
                        this.cursorPosition--;
                    }
                }
                this.textboxTextContent.nodeValue=this.textVal;
                this.setCursorPos();
                if (this.cursorPosition > 0) {
                    //retransform text element when cursor is at the left side of the box
                    if (this.supportsCharGeom) {
                        var cursorX = this.textboxText.getStartPositionOfChar(this.cursorPosition).x;
                    }
                    else {
                        var bbox = this.textboxText.getBBox();
                        var cursorX = bbox.x + bbox.width;
                    }
                    if ((cursorX + this.transX) < (this.x + this.textStyles["font-size"] / 3)) {
                        this.transX += (this.x + this.textStyles["font-size"] / 3) - (cursorX + this.transX);
                        if (this.transX * -1 < (this.boxWidth - this.textStyles["font-size"])) {
                            this.transX = 0;
                        }
                        this.textboxTextGroup.setAttributeNS(null,"transform","translate("+this.transX+",0)");
                    }
                }
                this.changed = true;
            }
        }
        //the two enter keys
         else if (keyCode == 10 || keyCode == 13) { // press return (enter)
            this.release();
        }
        //end key
        else if (keyCode == 35 && !(charCode)) {
            if (evt.shiftKey) {
                if (this.shiftDown == false) {
                    this.startOrigSelection = this.cursorPosition + 1;
                    this.startSelection = this.cursorPosition + 1;
                    this.shiftDown = true;
                }
            }
            this.cursorPosition = this.textVal.length - 1;
            this.setCursorPos();
            //if text string is too long
            var cursorX = parseInt(this.textboxCursor.getAttributeNS(null,"x1"));
            if ((cursorX + this.transX) > (this.x + this.boxWidth - this.textStyles["font-size"] / 3)) {
                this.transX = (this.x + this.boxWidth - this.textStyles["font-size"] / 3) - cursorX;
                this.textboxTextGroup.setAttributeNS(null,"transform","translate("+this.transX+",0)");
            }
            this.setCursorPos();
            if (evt.shiftKey) {
                if (this.shiftDown == false) {
                    this.startOrigSelection = this.cursorPosition;
                    this.startSelection = this.cursorPosition;
                    this.shiftDown = true;
                }
                this.endSelection = this.cursorPosition + 1;
                this.selectionRect.setAttributeNS(null,"display","inherit");
                this.selectionRectVisible = true;
                if (this.supportsCharGeom) {
                    var rectX = this.textboxText.getStartPositionOfChar(this.startSelection).x;
                    var width = (this.textboxText.getEndPositionOfChar(this.endSelection - 1).x - rectX);
                }
                else {
                    var bbox = this.textboxText.getBBox();
                    var rectX = this.x + this.textStyles["font-size"] / 3;
                    var width = this.x + bbox.width + this.textStyles["font-size"] / 3;
                }
                this.selectionRect.setAttributeNS(null,"x",rectX);        
                this.selectionRect.setAttributeNS(null,"width",width);
            }
            if (this.shiftDown && evt.shiftKey == false) {
                this.releaseShift();
            }
        }
        //home key
        else if (keyCode == 36 && !(charCode)) {
            if (evt.shiftKey) {
                if (this.shiftDown == false) {
                    this.startOrigSelection = this.cursorPosition + 1;
                    this.startSelection = this.cursorPosition + 1;
                    this.shiftDown = true;
                }
            }
            this.cursorPosition = -1;
            this.textboxText.setAttributeNS(null,"x",(this.x + this.textStyles["font-size"] / 3));
            this.textboxTextGroup.setAttributeNS(null,"transform","translate(0,0)");
            this.transX = 0;
            this.setCursorPos();
            if (evt.shiftKey) {
                if (this.shiftDown == false) {
                    this.startOrigSelection = this.cursorPosition;
                    this.startSelection = this.cursorPosition;
                    this.shiftDown = true;
                }
                this.endSelection = this.startSelection;
                this.startSelection = 0;
                this.selectionRect.setAttributeNS(null,"display","inherit");
                this.selectionRectVisible = true;
                if (this.supportsCharGeom) {
                    var rectX = this.textboxText.getStartPositionOfChar(this.startSelection).x;
                    var width = (this.textboxText.getEndPositionOfChar(this.endSelection - 1).x - rectX);
                }
                else {
                    var bbox = this.textboxText.getBBox();
                    var rectX = this.x + this.textStyles["font-size"] / 3;
                    var width = this.x + bbox.width + this.textStyles["font-size"] / 3;
                }
                this.selectionRect.setAttributeNS(null,"x",rectX);    
                this.selectionRect.setAttributeNS(null,"width",width);            
            }
            if (this.shiftDown && evt.shiftKey == false) {
                    this.releaseShift();
            }
        }
        //left key
        else if (keyCode == 37 && !(charCode)) {
            if (this.cursorPosition > -1) {
                this.cursorPosition--;
                this.setCursorPos();
                var cursorX = parseInt(this.textboxCursor.getAttributeNS(null,"x1"));
                if ((cursorX + this.transX) < (this.x + this.textStyles["font-size"] / 3)) {
                    this.transX += (this.x + this.textStyles["font-size"] / 3) - (cursorX + this.transX);
                    if (this.transX * -1 < (this.boxWidth - this.textStyles["font-size"])) {
                        this.transX = 0;
                    }
                    this.textboxTextGroup.setAttributeNS(null,"transform","translate("+this.transX+",0)");
                }
                //do selection if shift key is pressed
                if (evt.shiftKey && this.supportsCharGeom) {
                    if (this.shiftDown == false) {
                        this.startOrigSelection = this.cursorPosition + 2;
                        this.startSelection = this.cursorPosition + 2;
                        this.shiftDown = true;
                    }
                    this.endSelection = this.startOrigSelection;
                    this.startSelection = this.cursorPosition + 1;
                    this.selectionRect.setAttributeNS(null,"display","inherit");
                    this.selectionRectVisible = true;
                    var rectX = this.textboxText.getStartPositionOfChar(this.startSelection).x
                    this.selectionRect.setAttributeNS(null,"x",rectX);
                    this.selectionRect.setAttributeNS(null,"width",(this.textboxText.getEndPositionOfChar(this.endSelection - 1).x - rectX));
                }
                else {
                    if (this.shiftDown) {
                        this.releaseShift();
                    }
                }
            }
        }
        //right key
        else if (keyCode == 39 && !(charCode)) {
            if (this.cursorPosition < this.textVal.length - 1) {
                this.cursorPosition++;
                this.setCursorPos();
                var cursorX = parseInt(this.textboxCursor.getAttributeNS(null,"x1"));
                if ((cursorX + this.transX) > (this.x + this.boxWidth - this.textStyles["font-size"] / 3)) {
                    this.transX = (this.x + this.boxWidth - this.textStyles["font-size"] / 3) - cursorX;
                    this.textboxTextGroup.setAttributeNS(null,"transform","translate("+this.transX+",0)");
                }
                //do selection if shift key is pressed
                if (evt.shiftKey && this.supportsCharGeom) {
                    if (this.shiftDown == false) {
                        this.startOrigSelection = this.cursorPosition;
                        this.startSelection = this.cursorPosition;
                        this.shiftDown = true;
                    }
                    this.endSelection = this.cursorPosition + 1;
                    this.selectionRect.setAttributeNS(null,"display","inherit");
                    this.selectionRectVisible = true;
                    var rectX = this.textboxText.getStartPositionOfChar(this.startSelection).x
                    this.selectionRect.setAttributeNS(null,"x",rectX);
                    this.selectionRect.setAttributeNS(null,"width",(this.textboxText.getEndPositionOfChar(this.endSelection - 1).x - rectX));
                }
                else {
                    if (this.shiftDown) {
                        this.releaseShift();
                    }
                }
            }
        }
        //delete key
        else if ((keyCode == 127 || keyCode == 12 || keyCode == 46) && !(charCode)) {
            if ((this.textVal.length > 0) && (this.cursorPosition < (this.textVal.length))) {
                    var tempText = null;
                    if (this.shiftDown && evt.shiftKey == false && this.startSelection < this.textVal.length) {
                        //we need to delete selected text
                        var tempText = this.textVal.substring(0,this.startSelection) + this.textVal.substring(this.endSelection,this.textVal.length);
                        this.cursorPosition = this.startSelection - 1;
                        this.releaseShift();
                        this.changed = true;
                    }
                    else {
                        if (this.cursorPosition < (this.textVal.length - 1)) {
                            //we need to delete the next character, if cursor is not at the end of the textstring
                            var tempText = this.textVal.substring(0,(this.cursorPosition + 1)) + this.textVal.substring((this.cursorPosition + 2),(this.textVal.length));
                            this.changed = true;
                        }
                    }
                    if (this.changed) {
                        if(tempText != null) {
                            this.textVal = tempText;
                            this.textboxTextContent.nodeValue=this.textVal;
                            this.setCursorPos();
                        }
                    }
            }
        }
        //fire function if text changed
        if (myMapApp.navigator == "Adobe") {
            if (this.changed) {
                this.timer.setTimeout("fireFunction",this.timerMs);
            }
        }
}

textbox.prototype.setCursorPos = function() {
        //cursor is not at first position
        if (this.cursorPosition > -1) {
            if (this.supportsCharGeom) {
                if (this.textVal.length > 0) {
                    var cursorPos = this.textboxText.getEndPositionOfChar(this.cursorPosition).x;
                }
                else {
                    var cursorPos = (this.x + this.textStyles["font-size"] / 3);
                }    
                this.textboxCursor.setAttributeNS(null,"x1",cursorPos);
                this.textboxCursor.setAttributeNS(null,"x2",cursorPos);
            }
            else {
                //case MozillaSVG 1.5 or other SVG viewers not implementing .getEndPositionOfChar
                var bbox = this.textboxText.getBBox();
                this.textboxCursor.setAttributeNS(null,"x1",(bbox.x + bbox.width + this.textStyles["font-size"] / 3));
                this.textboxCursor.setAttributeNS(null,"x2",(bbox.x + bbox.width + this.textStyles["font-size"] / 3));
            }
        }
        else {
            //cursor is at first position
            //reset transformations
            this.textboxText.setAttributeNS(null,"x",(this.x + this.textStyles["font-size"] / 3));
            this.textboxTextGroup.setAttributeNS(null,"transform","translate(0,0)");
            this.transX = 0;
            if (this.supportsCharGeom) {
                if (this.textboxTextContent.length > 0) {
                    var cursorPos = this.textboxText.getStartPositionOfChar(0).x;
                }
                else {
                    var cursorPos = this.x + this.textStyles["font-size"] / 3;
                }
            }
            else {
                var cursorPos = this.x + this.textStyles["font-size"] / 3;
            }
            this.textboxCursor.setAttributeNS(null,"x1",cursorPos);
            this.textboxCursor.setAttributeNS(null,"x2",cursorPos);
        }
}

textbox.prototype.fireFunction = function() {
    var changeType = "change";
    if (this.textboxStatus == 0) {
        changeType = "release";
    }
    if (this.textboxStatus == 5) {
        this.textboxStatus = 0;
        changeType = "set";
    }
    if (typeof(this.functionToCall) == "function") {
        this.functionToCall(this.id,this.textVal,changeType);
    }
    if (typeof(this.functionToCall) == "object") {
        this.functionToCall.textboxChanged(this.id,this.textVal,changeType);    
    }
    if (typeof(this.functionToCall) == undefined) {
        return;
    }
}

textbox.prototype.getValue = function() {
    return this.textVal;
}    

textbox.prototype.setValue = function(value,fireFunction) {
    this.textVal = value.toString();
    this.textboxTextContent.nodeValue=this.textVal;
    //set the cursor to beginning and remove previous transforms
    this.cursorPosition = -1;
    this.setCursorPos();
    if (fireFunction == true) {
        this.textboxStatus = 5; //5 means is set by setValue
        this.fireFunction();
    }
}

textbox.prototype.release = function() {
    // set textbox status
    this.textboxStatus = 0;
    // remove event listeners
    document.documentElement.removeEventListener("keypress",this,false);
    if (myMapApp.navigator == "Adobe") {
        document.documentElement.removeEventListener("keydown",this,false);            
    }
    document.documentElement.removeEventListener("mousedown",this,false);
    document.documentElement.removeEventListener("mousemove",this,false);
    document.documentElement.removeEventListener("mouseup",this,false);
    //set cursor and text selection to invisible
    this.textboxCursor.setAttributeNS(null,"visibility","hidden");
    this.releaseShift();
    this.timer.setTimeout("fireFunction",this.timerMs);    
}

textbox.prototype.releaseShift = function() {
    this.selectionRect.setAttributeNS(null,"display","none");
    this.selectionRectVisible = false;
    this.shiftDown = false;    
}

textbox.prototype.calcCursorPosFromMouseEvt = function(evt) {
    //determine cursor position of mouse event
    var myCoords = myMapApp.calcCoord(evt,this.textboxText);
    //create an SVG Point object
    var mySVGPoint = document.documentElement.createSVGPoint();
    mySVGPoint.x = myCoords.x;
    mySVGPoint.y = myCoords.y;
    //set new cursor position
    if (this.textboxTextContent.length > 0) {
        if (this.supportsCharGeom) {
            //for regular SVG viewers that support .getCharNumAtPosition
            this.cursorPosition = this.textboxText.getCharNumAtPosition(mySVGPoint);
            if (this.cursorPosition > this.textVal.length - 1) {
                this.cursorPosition = this.textVal.length - 1;
            }
            //in this case the user did not correctly touch the text element
            if (this.cursorPosition == -1) {
                //first check if we can fix the position by moving the y-coordinate
                mySVGPoint.y = (this.textboxText.getBBox().y + this.textStyles["font-size"] * 0.5);
                this.cursorPosition = this.textboxText.getCharNumAtPosition(mySVGPoint);
                //check if cursor is to the right of the end of the text
                if (this.cursorPosition == -1) {
                    if (mySVGPoint.x > (this.textboxText.getBBox().x + this.textboxText.getBBox().width)) {
                        this.cursorPosition = this.textVal.length - 1;
                    }
                }
            }
        }
        else {
            //workaround for firefox 1.5/2.0 and other viewers not supporting .getCharNumAtPosition
            var bbox = this.textboxText.getBBox();
            var diffLeft = Math.abs(mySVGPoint.x - bbox.x);
            var diffRight = Math.abs(mySVGPoint.x - (bbox.x + bbox.width));
            if (diffLeft < diffRight) {
                this.cursorPosition = -1;
            }
            else {
                this.cursorPosition = this.textVal.length - 1;
            }
        }
    }
    else {
        //in case the text is empty
        this.cursorPosition = -1;                
    }    
}

textbox.prototype.moveTo = function(moveX,moveY) {
    this.x = moveX;
    this.y = moveY;
    //reposition textbox
    this.textboxRect.setAttributeNS(null,"x",this.x);
    this.textboxRect.setAttributeNS(null,"y",this.y);
    //reposition svg element
    this.svg.setAttributeNS(null,"x",this.x + this.textStyles["font-size"] / 4);
    this.svg.setAttributeNS(null,"y",this.y + this.boxHeight * 0.02);
    this.svg.setAttributeNS(null,"viewBox",(this.x + this.textStyles["font-size"] / 4)+" "+(this.y + this.boxHeight * 0.02)+" "+(this.boxWidth - (this.textStyles["font-size"]) / 2)+" "+(this.boxHeight * 0.96));
    //reposition text element
    this.textboxText.setAttributeNS(null,"x",(this.x + this.textStyles["font-size"] / 3));
    this.textboxText.setAttributeNS(null,"y",(this.y + this.textYOffset));
    //reposition selection element
    this.selectionRect.setAttributeNS(null,"x",(this.x + this.textStyles["font-size"] / 3));
    this.selectionRect.setAttributeNS(null,"y",(this.y + this.textYOffset - this.textStyles["font-size"] * 0.9));
    //reposition cursor
    this.textboxCursor.setAttributeNS(null,"x1",this.x);
    this.textboxCursor.setAttributeNS(null,"y1",(this.y + this.textYOffset + this.textStyles["font-size"] * 0.2));
    this.textboxCursor.setAttributeNS(null,"x2",this.x);
    this.textboxCursor.setAttributeNS(null,"y2",(this.y + this.textYOffset - this.textStyles["font-size"] * 0.9));
    //set the cursor to beginning and remove previous transforms
    this.cursorPosition = -1;
    this.setCursorPos();    
}

textbox.prototype.resize = function(newWidth) {
    this.boxWidth = newWidth;
    //resize textbox rectangle
    this.textboxRect.setAttributeNS(null,"width",this.boxWidth);
    //resize svg element
    this.svg.setAttributeNS(null,"width",this.boxWidth - (this.textStyles["font-size"]) / 2);
    this.svg.setAttributeNS(null,"viewBox",(this.x + this.textStyles["font-size"] / 4)+" "+(this.y + this.boxHeight * 0.02)+" "+(this.boxWidth - (this.textStyles["font-size"]) / 2)+" "+(this.boxHeight * 0.96));
    //set the cursor to beginning and remove previous transforms
    this.cursorPosition = -1;
    this.setCursorPos();    
}
