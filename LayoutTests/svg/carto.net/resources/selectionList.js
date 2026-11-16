/*
Scripts to create interactive selectionLists/Dropdown boxes in SVG using ECMA script
Copyright (C) <2006>  <Andreas Neumann>
Version 1.4, 2006-07-18
neumann@karto.baug.ethz.ch
http://www.carto.net/
http://www.carto.net/neumann/

Credits:
* Initial code was taken from Michel Hirtzler --> pilat.free.fr (thanks!)
* Thanks also to many people of svgdevelopers@yahoogroups.com
* Suggestions by Bruce Rindahl

----

Documentation: http://www.carto.net/papers/svg/gui/selectionlist/

----

current version: 1.4

version history:
0.99 (2004-10-26)
initial version

1.0 (2005-09-15)
allow undefined values in callback function for selectionlists that need not react on changes (suggestion by Bruce Rindahl)
allow selectionlist to be placed in a transformed group
now also works in MozillaSVG (due to changes in mapApp.js)

1.1 (2005-09-28)
removed "string" option in functionToCall (not important any more). function and object should be good enough
timer object from http://www.codingforums.com/showthread.php?s=&threadid=10531 is now used; attention: you need to link this script as well!
due to the new timer object we can now also mouse down and scroll more than one entry until we mouseup again, previously one had to click for forwarding each individual element

1.2 (2005-10-24)
removed style attributes and replaced them with individual XML attributes
initial tests on Opera 9 TP1, it seems to work except for the scrolling - due to a mouse-move event bug

1.2.1 (2005-02-07)
upgraded the documentation and fixed documentation errors

1.2.2 (2006-02-16)
added option to open selectionList to the top of the original box, this is useful for layouts where the selectionList is at the bottom of a page
added option to always put the selectionList on the top of it's parent group after activation in order to force the geometry to be on the top of all other geometry within the same group
improved background rectangle: these are now two rectangles: one on the bottom for the fill, one on the top for stroking
tested with Opera 9 TP2, works now fine, except keyboard input

1.2.3 (2006-02-21)
removed dependency on replaceSpecialChars (in file helper_functions.js) as this is not really needed in a true utf8 environment.
improved key event handling. People can now enter more than one key (within one second for each consequent key) and the list will jump to entries that match the keys; key events now also work in opera 9+ - however they sometimes collide with keyboard shortcuts.

1.3 (2006-03-11)
changed parameters of constructor (styling system): now an array of literals containing presentation attributes. Not hardcoded in selectionList.js anymore. Added check for number of arguments.

1.4 (2006-07-18)
added additional constructor parameter "parentNode", changed groupId to id, added methods .resize(width) and .moveTo(x,y)
improved scrolling (now also possible outside of the scrollbar, once the mouse is down)

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

original document site: http://www.carto.net/papers/svg/gui/selectionlist/
Please contact the author in case you want to use code or ideas commercially.
If you use this code, please include this copyright header, the included full
LGPL 2.1 text and read the terms provided in the LGPL 2.1 license
(http://www.gnu.org/copyleft/lesser.txt)

-------------------------------

If you wish, you can modify parameters (such as "look and feel") in the function "selectionList" (constructor).
You can adapt colors, fonts, cellpaddings, etc.

-------------------------------

Please report bugs and send improvements to neumann@karto.baug.ethz.ch
If you use this control, please link to the original (http://www.carto.net/papers/svg/gui/selectionlist/)
somewhere in the source-code-comment or the "about" of your project and give credits, thanks!

*/

function selectionList(id,parentNode,elementsArray,width,xOffset,yOffset,cellHeight,textPadding,heightNrElements,textStyles,boxStyles,scrollbarStyles,smallrectStyles,highlightStyles,triangleStyles,preSelect,openAbove,putOnTopOfParent,functionToCall) {
    var nrArguments = 19;
    var createSelbox= true;
    if (arguments.length == nrArguments) {
        //get constructor variables
        this.id = id; //the id or node reference of the group where the
        this.parentNode = parentNode; //can be of type string (id) or node reference (svg or g node)
        this.elementsArray = elementsArray; //the array containing the text elements of the selectionList
        this.width = width; //width of the selectionList in viewBox coordinates
        this.xOffset = xOffset; //upper left corner (xOffset) of selection list in viewBox coordinates
        this.yOffset = yOffset; //upper left corner (yOffset) of selection list in viewBox coordinates
        this.cellHeight = cellHeight; //cellHeight in viewBox coordinates
        this.heightNrElements = heightNrElements; //nr of elements for unfolded selectionList (number, integer)
        this.textStyles = textStyles; //array of literals containing presentation attributes for text
        if (!this.textStyles["font-size"]) {
            this.textStyles["font-size"] = "11";
        }
        this.boxStyles = boxStyles; //array of literals containing presentation attributes for the sel box
        if (!this.boxStyles["fill"]) {
            this.boxStyles["fill"] = "white"; //this fill value is also used for boxes under indiv. text elements (unselected status)
        }    
        if (!this.boxStyles["stroke"]) {
            this.boxStyles["stroke"] = "dimgray";
        }    
        if (!this.boxStyles["stroke-width"]) {
            this.boxStyles["stroke-width"] = 1;
        }
        this.scrollbarStyles = scrollbarStyles; //array of literals containing presentation attributes for the scrollbar
        this.smallrectStyles = smallrectStyles; //array of literals containing presentation attributes for the small rectangle next to selectbox
        this.highlightStyles = highlightStyles; //array of literals containing presentation attributes for the highlighted rectangles
        this.triangleStyles = triangleStyles; //array of literals containing presentation attributes for the triangle in the selectionList, also applies to fill of scroller
        this.preSelect = preSelect;
        this.openAbove = openAbove;
        this.putOnTopOfParent = putOnTopOfParent;
        this.functionToCall = functionToCall;

        //calculate other values
        this.triangleFourth = Math.round(this.cellHeight / 4); //this is only used internally
        this.textPadding = textPadding; //this is relative to the left of the cell
        this.textPaddingVertical = this.cellHeight - (this.cellHeight - this.textStyles["font-size"]) * 0.7; //this is relative to the top of the cell
        this.scrollerMinHeight = this.cellHeight * 0.5; //minimal height of the scroller rect
        
        //references
        this.scrollBar = undefined; //later a reference to the scrollbar rectangle
    
        //status variables
        this.activeSelection = this.preSelect; //holds currently selected index value
        this.listOpen = false; //status folded=false, open=true - previously been sLselectionVisible
        this.curLowerIndex = this.preSelect; //this value is adapted if the user moves scrollbar
        this.scrollStep = 0; //y-value to go for one element
        this.scrollerHeight = 0; //height of dragable scroller bar
        this.scrollActive = false; //determines if scrolling per up/down button is active
        this.panY = false; //stores the y value of event
        this.scrollCumulus = 0; //if value is less then a scrollstep we need to accumulate scroll values
        this.scrollDir = ""; //later holds "up" and "down"
        this.exists = true; //true means it exists, gets value false if method "removeSelectionList" is called
        this.pressedKeys = ""; //stores key events (pressed char values)
        this.keyTimer = undefined; //timer for resetting character strings after a given time period
    }
    else {
        createSelbox = false;
        alert("Error ("+id+"): wrong nr of arguments! You have to pass over "+nrArguments+" parameters.");
    }
    if (createSelbox) {
        //timer stuff
        this.timer = new Timer(this); //a Timer instance for calling the functionToCall
        this.timerMs = 200; //a constant of this object that is used in conjunction with the timer - functionToCall is called after 200 ms
        //createSelectionList
        this.createSelectionList();
    }
    else {
        alert("Could not create selectionlist with id '"+id+"' due to errors in the constructor parameters");    
    }
}

selectionList.prototype.createSelectionList = function() {
        var result = this.testParent();
        if (result) {
            //initial Rect, visible at the beginning
            var node = document.createElementNS(svgNS,"rect");
            node.setAttributeNS(null,"x",this.xOffset);
            node.setAttributeNS(null,"y",this.yOffset);
            node.setAttributeNS(null,"width",this.width);
            node.setAttributeNS(null,"height",this.cellHeight);
            for (var attrib in this.boxStyles) {
                node.setAttributeNS(null,attrib,this.boxStyles[attrib]);
            }        
            node.setAttributeNS(null,"id","selRect_"+this.id);
            node.addEventListener("click", this, false);
            this.parentGroup.appendChild(node);
            //initial text
            this.selectedText = document.createElementNS(svgNS,"text");
            this.selectedText.setAttributeNS(null,"x",this.xOffset + this.textPadding);
            this.selectedText.setAttributeNS(null,"y",this.yOffset + this.textPaddingVertical);
            var value = "";
            for (var attrib in this.textStyles) {
                value = this.textStyles[attrib];
                if (attrib == "font-size") {
                    value += "px";
                }
                this.selectedText.setAttributeNS(null,attrib,value);
            }        
            this.selectedText.setAttributeNS(null,"pointer-events","none");
            var selectionText = document.createTextNode(this.elementsArray[this.activeSelection]);
            this.selectedText.appendChild(selectionText);
            this.parentGroup.appendChild(this.selectedText);
            //small Rectangle to the right, onclick unfolds the selectionList
            var node = document.createElementNS(svgNS,"rect");
            node.setAttributeNS(null,"x",this.xOffset + this.width - this.cellHeight);
            node.setAttributeNS(null,"y",this.yOffset);
            node.setAttributeNS(null,"width",this.cellHeight);
            node.setAttributeNS(null,"height",this.cellHeight);
            for (var attrib in this.smallrectStyles) {
                node.setAttributeNS(null,attrib,this.smallrectStyles[attrib]);
            }        
            node.setAttributeNS(null,"id","selPulldown_"+this.id);
            node.addEventListener("click", this, false);
            this.parentGroup.appendChild(node);
            //triangle
            var node=document.createElementNS(svgNS,"path");
            var myTrianglePath = "M"+(this.xOffset + this.width - 3 * this.triangleFourth)+" "+(this.yOffset + this.triangleFourth)+" L"+(this.xOffset + this.width - this.triangleFourth)+" "+(this.yOffset + this.triangleFourth)+" L"+(this.xOffset + this.width - 2 * this.triangleFourth)+" "+(this.yOffset + 3 * this.triangleFourth)+" Z";
            node.setAttributeNS(null,"d",myTrianglePath);
            for (var attrib in this.triangleStyles) {
                node.setAttributeNS(null,attrib,this.triangleStyles[attrib]);
            }        
            node.setAttributeNS(null,"id","selTriangle_"+this.id);
            node.setAttributeNS(null,"pointer-events","none");
            this.parentGroup.appendChild(node);
            //rectangle below unfolded selectBox, at begin invisible
            this.rectBelowBox = document.createElementNS(svgNS,"rect");
            this.rectBelowBox.setAttributeNS(null,"x",this.xOffset);
            this.rectBelowBox.setAttributeNS(null,"y",this.yOffset + this.cellHeight);
            this.rectBelowBox.setAttributeNS(null,"width",this.width - this.cellHeight);
            this.rectBelowBox.setAttributeNS(null,"height",0);
            this.rectBelowBox.setAttributeNS(null,"fill",this.boxStyles["fill"]);
            this.rectBelowBox.setAttributeNS(null,"display","none");
            this.parentGroup.appendChild(this.rectBelowBox);
            //rectangle below unfolded selectBox, at begin invisible
            this.rectAboveBox = document.createElementNS(svgNS,"rect");
            this.rectAboveBox.setAttributeNS(null,"x",this.xOffset);
            this.rectAboveBox.setAttributeNS(null,"y",this.yOffset + this.cellHeight);
            this.rectAboveBox.setAttributeNS(null,"width",this.width - this.cellHeight);
            this.rectAboveBox.setAttributeNS(null,"height",0);
            this.rectAboveBox.setAttributeNS(null,"fill","none");
            this.rectAboveBox.setAttributeNS(null,"stroke",this.boxStyles["stroke"]);
            this.rectAboveBox.setAttributeNS(null,"stroke-width",this.boxStyles["stroke-width"]);
            this.rectAboveBox.setAttributeNS(null,"display","none");
            this.parentGroup.appendChild(this.rectAboveBox);
        }
        else {
            alert("could not create or reference 'parentNode' of selectionList with id '"+this.id+"'");
        }            
}

//test if parent group exists or create a new group at the end of the file
selectionList.prototype.testParent = function() {
    //test if of type object
    var nodeValid = false;
    if (typeof(this.parentNode) == "object") {
        if (this.parentNode.nodeName == "svg" || this.parentNode.nodeName == "g") {
            this.parentGroup = this.parentNode;
            nodeValid = true;
        }
    }
    else if (typeof(this.parentNode) == "string") { 
        //first test if Windows group exists
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

selectionList.prototype.handleEvent = function(evt) {
    var el = evt.currentTarget;
    var callerId = el.getAttributeNS(null,"id");
    var myRegExp = new RegExp(this.id);
    if (evt.type == "click") {
        if (myRegExp.test(callerId)) {
            if (callerId.match(/\bselPulldown|\bselRect/)) {
                if (this.listOpen == false) {
                    if (this.putOnTopOfParent) {
                        //put list on top of the parent group (bottom in the DOM tree)
                        this.parentGroup.parentNode.appendChild(this.parentGroup);
                    }
                    this.unfoldList();
                    this.listOpen = true;
                }
                else {
                    this.foldList();
                    this.listOpen = false;
                    evt.stopPropagation();
                }
            }
            if (callerId.match(/\bselHighlightSelection_/)) {
                this.selectAndClose(evt);
                evt.stopPropagation();
            }
        }
        else {
            //case that the event comes from the documentRoot element
            //but not from the selectionList itself
            if (!myRegExp.test(evt.target.getAttributeNS(null,"id"))) {
                this.foldList();
                this.listOpen = false;
            }
        }
    }
    if (evt.type == "mouseover") {
        if (myRegExp.test(callerId)) {
            if (callerId.match(/\bselHighlightSelection_/)) {
                for (var attrib in this.highlightStyles) {
                    el.setAttributeNS(null,attrib,this.highlightStyles[attrib]);
                }        
            }
        }
    }
    if (evt.type == "mouseout") {
        if (myRegExp.test(callerId)) {
            if (callerId.match(/\bselHighlightSelection_/) ){
                el.setAttributeNS(null,"fill",this.boxStyles["fill"]);
            }
            if (callerId.match(/\bselScrollbarRect_/)) {
                    this.scrollBarMove(evt);
            }
        }
    }
    if (evt.type == "mousedown") {
        if (myRegExp.test(callerId)) {
            if (callerId.match(/\bselScrollUpperRect_|\bselScrollLowerRect/)) {
                this.scrollDir = "down";
                this.scrollActive = true;
                if (callerId.match(/UpperRect/)) {
                    this.scrollDir = "up";
                }
                this.scroll(this.scrollDir,1,true); //1 is one element to scroll, true says that we should move scrollbar
                this.timer.setTimeout("scrollPerButton",400)
            }
            if (callerId.match(/\bselScrollbarRect_/)) {
                //add events to document.documentElement
                document.documentElement.addEventListener("mousemove",this,false);
                document.documentElement.addEventListener("mouseup",this,false);
                this.scrollBarMove(evt);
            }
        }
    }
    if (evt.type == "mouseup") {
        if (myRegExp.test(callerId)) {
            if (callerId.match(/\bselScrollUpperRect_|\bselScrollLowerRect/)) {
                this.scrollActive = false;
            }
        }
        if (el.nodeName == "svg" || el.nodeName == "svg:svg") {
            document.documentElement.removeEventListener("mousemove",this,false);
            document.documentElement.removeEventListener("mouseup",this,false);            
            this.scrollBarMove(evt);
        }
    }
    if (evt.type == "mousemove") {
        if (el.nodeName == "svg" || el.nodeName == "svg:svg") {
            this.scrollBarMove(evt);
        }
    }    //add keypress event
    if (evt.type == "keypress") {
        if (evt.charCode) {
            var character = String.fromCharCode(evt.charCode).toLowerCase();
        }
        else {
            //case opera and others
            var character = String.fromCharCode(evt.keyCode).toLowerCase();
            if (evt.keyCode == 0 || evt.keyCode == 16 || evt.keyCode == 17 || evt.keyCode == 18) {
                //shift key
                character = "";
            }
        }

        this.pressedKeys += character;

        if (character.length > 0) {
                if (this.keyTimer) {
                    this.timer.clearTimeout(this.keyTimer);
                }
                this.keyTimer = this.timer.setTimeout("resetPressedKeys",1000);
            this.scrollToKey();
        }
    }
}

selectionList.prototype.resetPressedKeys = function() {
     this.keyTimer = undefined;
     this.pressedKeys = "";
}

//this function is called when selectionList is unfolded
selectionList.prototype.unfoldList = function() {
    //create temporary group to hold temp geometry
    if (!this.dynamicTextGroup) {
        this.dynamicTextGroup = document.createElementNS(svgNS,"g");
        this.parentGroup.insertBefore(this.dynamicTextGroup,this.rectAboveBox);
    }
    var nrSelectionsOrig = this.elementsArray.length;
    if (this.heightNrElements < nrSelectionsOrig) {
        nrSelections = this.heightNrElements;
    }
    else {
        nrSelections = nrSelectionsOrig;
    }
    var selectionHeight = this.cellHeight * nrSelections;
    //build textElements from array
    //hold currentSelection Index to unroll list at new offset
    if ((nrSelectionsOrig - this.activeSelection) >= nrSelections) {
        this.curLowerIndex = this.activeSelection;
    }
    else {
        this.curLowerIndex = nrSelectionsOrig - nrSelections;
    }
    //following is a temporary YOffset that serves to distinquish between selectionLists that open above or below
    var YOffset = this.yOffset + this.cellHeight;
    if (this.openAbove) {
        YOffset = this.yOffset - this.heightNrElements * this.cellHeight;
    }
    //adopt background rectangle
    this.rectBelowBox.setAttributeNS(null,"height",selectionHeight);
    this.rectBelowBox.setAttributeNS(null,"display","inherit");
    this.rectBelowBox.setAttributeNS(null,"y",YOffset);
    this.rectAboveBox.setAttributeNS(null,"height",selectionHeight);
    this.rectAboveBox.setAttributeNS(null,"display","inherit");
    this.rectAboveBox.setAttributeNS(null,"y",YOffset);

    for (var i=0;i<nrSelections;i++) {
        //add rectangles to capture events
        var node = document.createElementNS(svgNS,"rect");
        node.setAttributeNS(null,"x",this.xOffset + this.textPadding / 2);
        node.setAttributeNS(null,"y",YOffset + this.cellHeight * i);
        node.setAttributeNS(null,"width",this.width - this.cellHeight - this.textPadding);
        node.setAttributeNS(null,"height",this.cellHeight);
        node.setAttributeNS(null,"fill",this.boxStyles["fill"]);
        node.setAttributeNS(null,"id","selHighlightSelection_" + this.id + "_" + (i + this.curLowerIndex));
        //add event-handler
        node.addEventListener("mouseover", this, false);
        node.addEventListener("mouseout", this, false);
        node.addEventListener("click", this, false);
        node.addEventListener("keypress", this, false);
        this.dynamicTextGroup.appendChild(node);
        //add text-elements
        var node = document.createElementNS(svgNS,"text");
        node.setAttributeNS(null,"id","selTexts_" + this.id + "_" + (i + this.curLowerIndex));
        node.setAttributeNS(null,"x",this.xOffset + this.textPadding);
        node.setAttributeNS(null,"y",YOffset + this.textPaddingVertical + this.cellHeight * i);
        node.setAttributeNS(null,"pointer-events","none");
        var value = "";
        for (var attrib in this.textStyles) {
            value = this.textStyles[attrib];
            if (attrib == "font-size") {
                value += "px";
            }
            node.setAttributeNS(null,attrib,value);
        }        
        var selectionText = document.createTextNode(this.elementsArray[this.curLowerIndex + i]);
        node.appendChild(selectionText);
        this.dynamicTextGroup.appendChild(node);
    }

    //create Scrollbar
    if (this.heightNrElements < nrSelectionsOrig) {
        //calculate scrollstep
        this.scrollerHeight = (this.heightNrElements / nrSelectionsOrig) * (selectionHeight - 2 * this.cellHeight);
        if (this.scrollerHeight < this.scrollerMinHeight) {
            this.scrollerHeight = this.scrollerMinHeight;
        }
        this.scrollStep = (selectionHeight - 2 * this.cellHeight - this.scrollerHeight) / (nrSelectionsOrig - this.heightNrElements);
        //scrollbar
        this.scrollBar = document.createElementNS(svgNS,"rect");
        this.scrollBar.setAttributeNS(null,"x",this.xOffset + this.width - this.cellHeight);
        this.scrollBar.setAttributeNS(null,"y",YOffset + this.cellHeight);
        this.scrollBar.setAttributeNS(null,"width",this.cellHeight);
        this.scrollBar.setAttributeNS(null,"height",selectionHeight - this.cellHeight * 2);
        for (var attrib in this.scrollbarStyles) {
            this.scrollBar.setAttributeNS(null,attrib,this.scrollbarStyles[attrib]);
        }        
        this.scrollBar.setAttributeNS(null,"id","selScrollbarRect_"+this.id);
        this.scrollBar.addEventListener("mousedown", this, false);
        //node.addEventListener("mouseup", this, false);
        //node.addEventListener("mousemove", this, false);
        //node.addEventListener("mouseout", this, false);
        this.dynamicTextGroup.appendChild(this.scrollBar);
        //upper rectangle
        var node = document.createElementNS(svgNS,"rect");
        node.setAttributeNS(null,"x",this.xOffset + this.width - this.cellHeight);
        node.setAttributeNS(null,"y",YOffset);
        node.setAttributeNS(null,"width",this.cellHeight);
        node.setAttributeNS(null,"height",this.cellHeight);
        for (var attrib in this.smallrectStyles) {
            node.setAttributeNS(null,attrib,this.smallrectStyles[attrib]);
        }        
        node.setAttributeNS(null,"id","selScrollUpperRect_"+this.id);
        node.addEventListener("mousedown", this, false);
        node.addEventListener("mouseup", this, false);
        this.dynamicTextGroup.appendChild(node);
        //upper triangle
        var node = document.createElementNS(svgNS,"path");
        var myPath = "M"+(this.xOffset + this.width - 3 * this.triangleFourth)+" "+(YOffset + 3 * this.triangleFourth)+" L"+(this.xOffset + this.width - this.triangleFourth)+" "+(YOffset + 3 * this.triangleFourth)+" L"+(this.xOffset + this.width - 2 * this.triangleFourth)+" "+(YOffset + this.triangleFourth)+" Z";
        node.setAttributeNS(null,"d",myPath);
        for (var attrib in this.triangleStyles) {
            node.setAttributeNS(null,attrib,this.triangleStyles[attrib]);
        }        
        node.setAttributeNS(null,"pointer-events","none");
        this.dynamicTextGroup.appendChild(node);
        //lower rectangle
        var node = document.createElementNS(svgNS,"rect");
        node.setAttributeNS(null,"x",this.xOffset + this.width - this.cellHeight);
        node.setAttributeNS(null,"y",YOffset - this.cellHeight + selectionHeight);
        node.setAttributeNS(null,"width",this.cellHeight);
        node.setAttributeNS(null,"height",this.cellHeight);
        for (var attrib in this.smallrectStyles) {
            node.setAttributeNS(null,attrib,this.smallrectStyles[attrib]);
        }        
        node.setAttributeNS(null,"id","selScrollLowerRect_" + this.id);
        node.addEventListener("mousedown", this, false);
        node.addEventListener("mouseup", this, false);
        this.dynamicTextGroup.appendChild(node);
        //lower triangle
        var node = document.createElementNS(svgNS,"path");
        var myPath = "M"+(this.xOffset + this.width - 3 * this.triangleFourth)+" "+(YOffset - this.cellHeight + selectionHeight + this.triangleFourth)+" L"+(this.xOffset + this.width - this.triangleFourth)+" "+(YOffset - this.cellHeight + selectionHeight + this.triangleFourth)+" L"+(this.xOffset + this.width - 2 * this.triangleFourth)+" "+(YOffset - this.cellHeight + selectionHeight + 3 * this.triangleFourth)+" Z";
        node.setAttributeNS(null,"d",myPath);
        for (var attrib in this.triangleStyles) {
            node.setAttributeNS(null,attrib,this.triangleStyles[attrib]);
        }        
        node.setAttributeNS(null,"pointer-events","none");
        this.dynamicTextGroup.appendChild(node);
        //scrollerRect
        var node = document.createElementNS(svgNS,"rect");
        node.setAttributeNS(null,"x",this.xOffset + this.width - this.cellHeight);
        node.setAttributeNS(null,"y",YOffset + this.cellHeight + this.scrollStep * this.curLowerIndex);
        node.setAttributeNS(null,"width",this.cellHeight);
        node.setAttributeNS(null,"height",this.scrollerHeight);
        for (var attrib in this.smallrectStyles) {
            node.setAttributeNS(null,attrib,this.smallrectStyles[attrib]);
        }        
        node.setAttributeNS(null,"pointer-events","none");
        node.setAttributeNS(null,"id","selScroller_"+this.id);
        this.dynamicTextGroup.appendChild(node);
    }
    //add event handler to root element to close selectionList if one clicks outside
    document.documentElement.addEventListener("click",this,false);
    document.documentElement.addEventListener("keypress",this,false);
}

//this function folds/hides the selectionList again
selectionList.prototype.foldList = function() {
    this.parentGroup.removeChild(this.dynamicTextGroup);
    this.dynamicTextGroup = null;
    this.rectBelowBox.setAttributeNS(null,"display","none");
    this.rectAboveBox.setAttributeNS(null,"display","none");
    document.documentElement.removeEventListener("click",this,false);
    document.documentElement.removeEventListener("keypress",this,false);
    this.scrollBar = undefined;
}

selectionList.prototype.selectAndClose = function(evt) {
    var mySelEl = evt.target;
    var result = mySelEl.getAttributeNS(null,"id").split("_");
    this.activeSelection = parseInt(result[2]);
    this.curLowerIndex = this.activeSelection;
    this.foldList();
    this.listOpen = false;
    this.selectedText.firstChild.nodeValue = this.elementsArray[this.activeSelection];
    this.timer.setTimeout("fireFunction",this.timerMs);
}

selectionList.prototype.fireFunction = function() {
    if (typeof(this.functionToCall) == "function") {
        this.functionToCall(this.id,this.activeSelection,this.elementsArray[this.activeSelection]);
    }
    if (typeof(this.functionToCall) == "object") {
        this.functionToCall.getSelectionListVal(this.id,this.activeSelection,this.elementsArray[this.activeSelection]);
    }
    //suggestion by Bruce Rindahl
    //allows to initialize a selectionlist without a specific callback function on changing the selectionlist entry
    if (typeof(this.functionToCall) == undefined) {
        return;
    }
}

selectionList.prototype.scrollPerButton = function() {
    if (this.scrollActive == true) {
        this.scroll(this.scrollDir,1,true); //1 is one element to scroll, true says that we should move scrollbar
        this.timer.setTimeout("scrollPerButton",100)
    }
}

selectionList.prototype.scroll = function(scrollDir,scrollNr,scrollBool) {
    var nrSelections = this.elementsArray.length;
    var scroller = document.getElementById("selScroller_"+this.id);
    //following is a temporary YOffset that serves to distinquish between selectionLists that open above or below
    var YOffset = this.yOffset + this.cellHeight;
    if (this.openAbove) {
        YOffset = this.yOffset - this.heightNrElements * this.cellHeight;
    }
    if (scrollNr < this.heightNrElements) {
        if ((this.curLowerIndex > 0) && (scrollDir == "up")) {
            if (scrollNr > this.curLowerIndex) {
                scrollNr = this.curLowerIndex;
            }
            //decrement current index
            this.curLowerIndex = this.curLowerIndex - scrollNr;
            //move scroller
            if (scrollBool == true) {
                scroller.setAttributeNS(null,"y",parseFloat(scroller.getAttributeNS(null,"y"))+ this.scrollStep * -1);
            }
            //add upper rect elements
            for (var i=0;i<scrollNr;i++) {
                var node = document.createElementNS(svgNS,"rect");
                node.setAttributeNS(null,"x",this.xOffset + this.textPadding / 2);
                node.setAttributeNS(null,"y",YOffset + this.cellHeight * i);
                node.setAttributeNS(null,"width",this.width - this.cellHeight - this.textPadding);
                node.setAttributeNS(null,"height",this.cellHeight);
                node.setAttributeNS(null,"fill",this.boxStyles["fill"]);
                node.setAttributeNS(null,"id","selHighlightSelection_" + this.id + "_" + (i + this.curLowerIndex));
                //add event-handler
                node.addEventListener("mouseover", this, false);
                node.addEventListener("mouseout", this, false);
                node.addEventListener("click", this, false);
                node.addEventListener("keypress", this, false);
                this.dynamicTextGroup.appendChild(node);
                //add text-nodes
                var node = document.createElementNS(svgNS,"text");
                node.setAttributeNS(null,"id","selTexts_" + this.id + "_" + (i + this.curLowerIndex));
                node.setAttributeNS(null,"x",this.xOffset + this.textPadding);
                node.setAttributeNS(null,"y",YOffset + this.textPaddingVertical + this.cellHeight * i);
                node.setAttributeNS(null,"pointer-events","none");
                var value = "";
                for (var attrib in this.textStyles) {
                    value = this.textStyles[attrib];
                    if (attrib == "font-size") {
                        value += "px";
                    }
                    node.setAttributeNS(null,attrib,value);
                }        
                var selectionText = document.createTextNode(this.elementsArray[this.curLowerIndex + i]);
                node.appendChild(selectionText);
                this.dynamicTextGroup.appendChild(node);
            }
            //move middle elements
            for (var j=i;j<this.heightNrElements;j++) {
                var node = document.getElementById("selTexts_" + this.id + "_" + (j + this.curLowerIndex));
                node.setAttributeNS(null,"y",parseFloat(node.getAttributeNS(null,"y")) + (this.cellHeight * scrollNr));
                var node = document.getElementById("selHighlightSelection_" + this.id + "_" + (j + this.curLowerIndex));
                node.setAttributeNS(null,"y",parseFloat(node.getAttributeNS(null,"y")) + (this.cellHeight * scrollNr));
            }
            //remove lower elements
            for (var k=j;k<(j+scrollNr);k++) {
                var node = document.getElementById("selTexts_" + this.id + "_" + (k + this.curLowerIndex));
                this.dynamicTextGroup.removeChild(node);
                var node = document.getElementById("selHighlightSelection_" + this.id +"_" + (k + this.curLowerIndex));
                this.dynamicTextGroup.removeChild(node);
            }
        }
        else if ((this.curLowerIndex < nrSelections - this.heightNrElements) && (scrollDir == "down")) {
            //move Scroller
            if (scrollBool == true) {
                scroller.setAttributeNS(null,"y",parseFloat(scroller.getAttributeNS(null,"y")) + this.scrollStep);
            }
            //remove most upper element
            for (var i=0;i<scrollNr;i++) {
                var node = document.getElementById("selTexts_" + this.id + "_" + (this.curLowerIndex + i));
                this.dynamicTextGroup.removeChild(node);
                var node = document.getElementById("selHighlightSelection_" + this.id + "_" + (this.curLowerIndex + i));
                this.dynamicTextGroup.removeChild(node);
            }
            //move middle elements
            for (var j=i;j<this.heightNrElements;j++) {
                var node = document.getElementById("selTexts_" + this.id + "_" + (j + this.curLowerIndex));
                node.setAttributeNS(null,"y",parseFloat(node.getAttributeNS(null,"y")) - (this.cellHeight * scrollNr));
                var node = document.getElementById("selHighlightSelection_" + this.id + "_" + (j + this.curLowerIndex));
                node.setAttributeNS(null,"y",parseFloat(node.getAttributeNS(null,"y")) - (this.cellHeight * scrollNr));
            }
            //add most lower element
            for (var k=j;k<(j+scrollNr);k++) {
                var node = document.createElementNS(svgNS,"rect");
                node.setAttributeNS(null,"x",this.xOffset + this.textPadding / 2);
                node.setAttributeNS(null,"y",YOffset + this.cellHeight * (k - scrollNr));
                node.setAttributeNS(null,"width",this.width - this.cellHeight - this.textPadding);
                node.setAttributeNS(null,"height",this.cellHeight);
                node.setAttributeNS(null,"fill",this.boxStyles["fill"]);
                node.setAttribute("id","selHighlightSelection_" + this.id + "_" + (k + this.curLowerIndex));
                //add event-handler
                node.addEventListener("mouseover", this, false);
                node.addEventListener("mouseout", this, false);
                node.addEventListener("click", this, false);
                node.addEventListener("keypress", this, false);
                this.dynamicTextGroup.appendChild(node);
                //add text-nodes
                var node = document.createElementNS(svgNS,"text");
                node.setAttributeNS(null,"id","selTexts_" + this.id + "_" + (k + this.curLowerIndex));
                node.setAttributeNS(null,"x",this.xOffset + this.textPadding);
                node.setAttributeNS(null,"y",YOffset + this.textPaddingVertical + this.cellHeight * (k - scrollNr));
                node.setAttributeNS(null,"pointer-events","none");
                var value = "";
                for (var attrib in this.textStyles) {
                    value = this.textStyles[attrib];
                    if (attrib == "font-size") {
                        value += "px";
                    }
                    node.setAttributeNS(null,attrib,value);
                }        
                var selectionText = document.createTextNode(this.elementsArray[this.curLowerIndex + k]);
                node.appendChild(selectionText);
                this.dynamicTextGroup.appendChild(node);
            }
            //increment current index
            this.curLowerIndex = this.curLowerIndex + scrollNr;
        }
    }
    else {
            //remove lower elements
            for (var i=0;i<this.heightNrElements;i++) {
                var node = document.getElementById("selTexts_" + this.id + "_" + (i + this.curLowerIndex));
                this.dynamicTextGroup.removeChild(node);
                var node = document.getElementById("selHighlightSelection_" + this.id +"_" + (i + this.curLowerIndex));
                this.dynamicTextGroup.removeChild(node);
            }
            if (scrollDir == "down") {
                this.curLowerIndex = this.curLowerIndex + scrollNr;
            }
            else {
                this.curLowerIndex = this.curLowerIndex - scrollNr;
            }
            for (var i=0;i<this.heightNrElements;i++) {
                var node = document.createElementNS(svgNS,"rect");
                node.setAttributeNS(null,"x",this.xOffset + this.textPadding / 2);
                node.setAttributeNS(null,"y",YOffset + this.cellHeight * i);
                node.setAttributeNS(null,"width",this.width - this.cellHeight - this.textPadding);
                node.setAttributeNS(null,"height",this.cellHeight);
                node.setAttributeNS(null,"fill",this.boxStyles["fill"]);
                node.setAttributeNS(null,"id","selHighlightSelection_" + this.id + "_" + (i + this.curLowerIndex));
                //add event-handler
                node.addEventListener("mouseover", this, false);
                node.addEventListener("mouseout", this, false);
                node.addEventListener("click", this, false);
                node.addEventListener("keypress", this, false);
                this.dynamicTextGroup.appendChild(node);
                //add text-nodes
                var node = document.createElementNS(svgNS,"text");
                node.setAttributeNS(null,"id","selTexts_" + this.id + "_" + (i + this.curLowerIndex));
                node.setAttributeNS(null,"x",this.xOffset + this.textPadding);
                node.setAttributeNS(null,"y",YOffset + this.textPaddingVertical + this.cellHeight * i);
                node.setAttributeNS(null,"pointer-events","none");
                var value = "";
                for (var attrib in this.textStyles) {
                    value = this.textStyles[attrib];
                    if (attrib == "font-size") {
                        value += "px";
                    }
                    node.setAttributeNS(null,attrib,value);
                }        
                var selectionText = document.createTextNode(this.elementsArray[this.curLowerIndex + i]);
                node.appendChild(selectionText);
                this.dynamicTextGroup.appendChild(node);
            }
    }
}

//event listener for Scrollbar element
selectionList.prototype.scrollBarMove = function(evt) {
    var scroller = document.getElementById("selScroller_" + this.id);
    var scrollerHeight = parseFloat(scroller.getAttributeNS(null,"height"));
    var scrollerMinY = parseFloat(this.scrollBar.getAttributeNS(null,"y"));
    var scrollerMaxY = parseFloat(this.scrollBar.getAttributeNS(null,"y")) + parseFloat(this.scrollBar.getAttributeNS(null,"height")) - scrollerHeight;
    //following is a temporary YOffset that serves to distinquish between selectionLists that open above or below
    var YOffset = this.yOffset + this.cellHeight;
    if (this.openAbove) {
        YOffset = this.yOffset - this.heightNrElements * this.cellHeight;
    }
    if (evt.type == "mousedown") {
        this.panStatus = true;
        for (var attrib in this.triangleStyles) {
            scroller.setAttributeNS(null,attrib,this.triangleStyles[attrib]);
        }        
        var coordPoint = myMapApp.calcCoord(evt,this.scrollBar);
        this.panY = coordPoint.y;
        var oldY = parseFloat(scroller.getAttributeNS(null,"y"));
        var newY = this.panY - parseFloat(scroller.getAttributeNS(null,"height")) / 2;
        if (newY < scrollerMinY) {
            newY = scrollerMinY;
            //maybe recalculate this.panY ??
        }
        if (newY > scrollerMaxY) {
            newY = scrollerMaxY;
        }
        var panDiffY = newY - oldY;
        var scrollDir = "down";
        this.scrollCumulus = 0;
        if(Math.abs(panDiffY) > this.scrollStep) {
            var scrollNr = Math.abs(Math.round(panDiffY / this.scrollStep));
            if (panDiffY > 0) {
                this.scrollCumulus = panDiffY - this.scrollStep * scrollNr;
            }
            else {
                this.scrollCumulus = panDiffY + this.scrollStep * scrollNr;
                scrollDir = "up";
            }
            newY = oldY + panDiffY;
            scroller.setAttributeNS(null,"y",newY);
            this.scroll(scrollDir,scrollNr,false);
        }
    }
    if (evt.type == "mouseup" || evt.type == "mouseout") {
        if (this.panStatus == true) {
            var newY = parseFloat(scroller.getAttributeNS(null,"y"));
            for (var attrib in this.smallrectStyles) {
                scroller.setAttributeNS(null,attrib,this.smallrectStyles[attrib]);
            }        
            scroller.setAttributeNS(null,"y",YOffset + this.cellHeight + this.scrollStep * this.curLowerIndex);
        }
        this.panStatus = false;
    }
    if (evt.type == "mousemove") {
        if (this.panStatus == true) {
            var coordPoint = myMapApp.calcCoord(evt,this.scrollBar);
            var panNewEvtY = coordPoint.y;
                var panDiffY = panNewEvtY - this.panY;
                var oldY = parseFloat(scroller.getAttributeNS(null,"y"));
                var newY = oldY + panDiffY;
                if (newY < scrollerMinY) {
                    newY = scrollerMinY;
                }
                if (newY > scrollerMaxY) {
                    newY = scrollerMaxY;
                }
            var panDiffY = newY - oldY;
            if ((panDiffY < 0 && panNewEvtY <= (scrollerMaxY+scrollerHeight)) || (panDiffY > 0 && panNewEvtY >= scrollerMinY)) {
                this.scrollCumulus += panDiffY;
                var scrollDir = "down";
                var scrollNr = 0;
                if(Math.abs(this.scrollCumulus) >= this.scrollStep) {
                    scrollNr = Math.abs(Math.round(this.scrollCumulus / this.scrollStep));
                    if (this.scrollCumulus > 0) {
                        this.scrollCumulus = this.scrollCumulus - this.scrollStep * scrollNr;
                    }
                    else {
                        this.scrollCumulus = this.scrollCumulus + this.scrollStep * scrollNr;
                        scrollDir = "up";
                    }
                    this.scroll(scrollDir,scrollNr,false);
                }
                else {
                    if (Math.abs(this.scrollCumulus) > this.scrollStep) {
                        scrollNr = 1;
                        if (panDiffY < 0) {
                            scrollDir = "up";
                            this.scrollCumulus = this.scrollCumulus + this.scrollStep;
                        }
                        else {
                            this.scrollCumulus = this.scrollCumulus - this.scrollStep;
                        }
                        panDiffY = this.scrollCumulus;
                        this.scroll(scrollDir,scrollNr,false);
                    }
                    else {
                        if (newY == scrollerMinY && this.curLowerIndex != 0) {
                            this.scroll("up",1,false);
                        }
                        else if (newY == scrollerMaxY && this.curLowerIndex != (this.elementsArray.length - this.heightNrElements)) {
                            this.scroll("down",1,false);
                        }
                    }
                }
                newY = oldY + panDiffY;
                scroller.setAttributeNS(null,"y",newY);
                this.panY = panNewEvtY;
            }
        }
    }
}

selectionList.prototype.scrollToKey = function(pressedKey) {
    var oldActiveSelection = this.activeSelection;
    for (var i=0;i<this.elementsArray.length;i++) {
        if (this.elementsArray[i].toLowerCase().substr(0,this.pressedKeys.length) == this.pressedKeys) {
            if (this.listOpen == true) {
                this.foldList();
            }
            this.activeSelection = i;
            this.unfoldList();
            this.listOpen = true;
            this.activeSelection = oldActiveSelection;
            break;
        }
    }
}

selectionList.prototype.elementExists = function(elementName) {
    var result = -1;
    for (i=0;i<this.elementsArray.length;i++) {
        if (this.elementsArray[i] == elementName) {
            result = i;
        }
    }
    return result;
}

selectionList.prototype.selectElementByName = function(elementName,fireFunction) {
    //fireFunction: (true|false); determines whether to execute selectFunction or not
    existsPosition = this.elementExists(elementName);
    if (existsPosition != -1) {
        if (this.listOpen == true) {
            this.foldList();
        }
        this.activeSelection = existsPosition;
        this.selectedText.firstChild.nodeValue = this.elementsArray[this.activeSelection];
        if (this.listOpen == true) {
            this.unfoldList();
        }
        if (fireFunction == true) {
            this.fireFunction();
        }
    }
    return existsPosition;
}

selectionList.prototype.selectElementByPosition = function(position,fireFunction) {
    //fireFunction: (true|false); determines whether to execute selectFunction or not
    if (position < this.elementsArray.length) {
        if (this.listOpen == true) {
            this.foldList();
        }
        this.activeSelection = position;
        this.selectedText.firstChild.nodeValue = this.elementsArray[this.activeSelection];
        if (this.listOpen == true) {
            this.unfoldList();
        }
        if (fireFunction == true) {
            this.fireFunction();
        }
    }
    else {
        position = -1;
    }
    return position;
}

selectionList.prototype.sortList = function(direction) {
    //direction: asc|desc, for ascending or descending
    var mySelElementString = this.elementsArray[this.activeSelection];
    if (this.listOpen == true) {
        this.foldList();
    }
    this.elementsArray.sort();
    if (direction == "desc") {
        this.elementsArray.reverse();
    }
    this.activeSelection = this.elementExists(mySelElementString);
    if (this.listOpen == true) {
        this.unfoldList();
    }

    return direction;
}

selectionList.prototype.deleteElement = function(elementName) {
    existsPosition = this.elementExists(elementName);
    if (existsPosition != -1) {
        if (this.listOpen == true) {
            this.foldList();
        }
        var tempArray = new Array;
        tempArray = tempArray.concat(this.elementsArray.slice(0,existsPosition),this.elementsArray.slice(existsPosition + 1,this.elementsArray.length));
        this.elementsArray = tempArray;
        if (this.activeSelection == existsPosition) {
            this.selectedText.firstChild.nodeValue = this.elementsArray[this.activeSelection];
        }
        if (this.activeSelection > existsPosition) {
            this.activeSelection -= 1;
        }
        if (this.listOpen == true) {
            this.unfoldList();
        }
    }
    return existsPosition;
}

selectionList.prototype.addElementAtPosition = function(elementName,position) {
    if (position > this.elementsArray.length) {
        this.elementsArray.push(elementName);
        position = this.elementsArray.length - 1;
    }
    else {
        var tempArray = new Array;
        tempArray = tempArray.concat(this.elementsArray.slice(0,position),elementName,this.elementsArray.slice(position,this.elementsArray.length));
        this.elementsArray = tempArray;
    }
    if (this.listOpen == true) {
        this.foldList();
    }
    if (position <= this.activeSelection) {
        this.activeSelection += 1;
    }
    if (this.listOpen == true) {
        this.unfoldList();
    }
    return position;
}

selectionList.prototype.getCurrentSelectionElement = function() {
    return this.elementsArray[this.activeSelection];
}

selectionList.prototype.getCurrentSelectionIndex = function() {
    return this.activeSelection;
}

selectionList.prototype.removeSelectionList = function() {
    //remove all Elements of selectionList
    this.exists = false;
    while (this.parentGroup.hasChildNodes()) {
        this.parentGroup.removeChild(this.parentGroup.firstChild);
    }
}

selectionList.prototype.moveTo = function(moveX,moveY) {
    this.xOffset = moveX;
    this.yOffset = moveY;
    //reposition initial rectangle
    var initRect = document.getElementById("selRect_"+this.id);
    initRect.setAttributeNS(null,"x",this.xOffset);
    initRect.setAttributeNS(null,"y",this.yOffset);
    //reposition initial text
    this.selectedText.setAttributeNS(null,"x",this.xOffset + this.textPadding);
    this.selectedText.setAttributeNS(null,"y",this.yOffset + this.textPaddingVertical);
    //reposition small rectangle to the right
    var smallRectRight = document.getElementById("selPulldown_"+this.id);
    smallRectRight.setAttributeNS(null,"x",this.xOffset + this.width - this.cellHeight);
    smallRectRight.setAttributeNS(null,"y",this.yOffset);
    //reposition triangle
    var triangle = document.getElementById("selTriangle_"+this.id);
    var myTrianglePath = "M"+(this.xOffset + this.width - 3 * this.triangleFourth)+" "+(this.yOffset + this.triangleFourth)+" L"+(this.xOffset + this.width - this.triangleFourth)+" "+(this.yOffset + this.triangleFourth)+" L"+(this.xOffset + this.width - 2 * this.triangleFourth)+" "+(this.yOffset + 3 * this.triangleFourth)+" Z";
    triangle.setAttributeNS(null,"d",myTrianglePath);    
    //change positions of rectbox below and above
    this.rectBelowBox.setAttributeNS(null,"x",this.xOffset);
    this.rectBelowBox.setAttributeNS(null,"y",this.yOffset + this.cellHeight);
    this.rectAboveBox.setAttributeNS(null,"x",this.xOffset);
    this.rectAboveBox.setAttributeNS(null,"y",this.yOffset + this.cellHeight);
}

selectionList.prototype.resize = function(newWidth) {
    this.width = newWidth;
    //reposition initial rectangle
    var initRect = document.getElementById("selRect_"+this.id);
    initRect.setAttributeNS(null,"width",this.width);
    //reposition small rectangle to the right
    var smallRectRight = document.getElementById("selPulldown_"+this.id);
    smallRectRight.setAttributeNS(null,"x",this.xOffset + this.width - this.cellHeight);
    //reposition triangle
    var triangle = document.getElementById("selTriangle_"+this.id);
    var myTrianglePath = "M"+(this.xOffset + this.width - 3 * this.triangleFourth)+" "+(this.yOffset + this.triangleFourth)+" L"+(this.xOffset + this.width - this.triangleFourth)+" "+(this.yOffset + this.triangleFourth)+" L"+(this.xOffset + this.width - 2 * this.triangleFourth)+" "+(this.yOffset + 3 * this.triangleFourth)+" Z";
    triangle.setAttributeNS(null,"d",myTrianglePath);    
    //change sizes of rectbox below and above
    this.rectBelowBox.setAttributeNS(null,"width",this.width - this.cellHeight);
    this.rectAboveBox.setAttributeNS(null,"width",this.width - this.cellHeight);
}
