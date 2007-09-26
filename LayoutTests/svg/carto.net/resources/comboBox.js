/*
Scripts to create interactive comboboxes in SVG using ECMA script
Copyright (C) <2006>  <Andreas Neumann>
Version 1.2.1, 2006-10-03
neumann@karto.baug.ethz.ch
http://www.carto.net/
http://www.carto.net/neumann/

Credits:

----

current version: 1.2.1

version history:
1.0 (2006-02-20)
initial version

1.1 (2006-05-17)
decoupled styling from this file, fixes event handling so people can scroll outside scroller if mouse is down (events now added to root element)

1.2 (2006-10-02)
changed internally "groupId" to "id" to make it consistent with other GUI elements, introduced "parentId" as a new constructor parameter for the same reason, introduced new methods "resize()" and "moveTo()"

1.2.1 (2006-10-03)
corrected a slight bug regarding DOM hierarchy of created elements

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

original document site: http://www.carto.net/papers/svg/gui/combobox/
Please contact the author in case you want to use code or ideas commercially.
If you use this code, please include this copyright header, the included full
LGPL 2.1 text and read the terms provided in the LGPL 2.1 license
(http://www.gnu.org/copyleft/lesser.txt)

-------------------------------

If you wish, you can modify parameters (such as "look and feel") in the function "combobox" (constructor).
You can adapt colors, fonts, cellpaddings, etc.

-------------------------------

Please report bugs and send improvements to neumann@karto.baug.ethz.ch
If you use this control, please link to the original (http://www.carto.net/papers/svg/gui/combobox/)
somewhere in the source-code-comment or the "about" of your project and give credits, thanks!

*/

function combobox(id,parentNode,elementsArray,width,xOffset,yOffset,cellHeight,textPadding,heightNrElements,multiple,offsetValue,textStyles,boxStyles,scrollbarStyles,smallrectStyles,highlightStyles,triangleStyles,functionToCall) {
    var nrArguments = 18;
    var createCombobox= true;
    if (arguments.length == nrArguments) {
        //get constructor variables
        this.id = id;
        this.parentNode = parentNode;
        this.elementsArray = elementsArray;
        this.width = width;
        this.xOffset = xOffset;
        this.yOffset = yOffset;
        this.cellHeight = cellHeight; //cellHeight in viewBox coordinates
        this.textPadding = textPadding; //this is relative to the left of the cell
        this.heightNrElements = heightNrElements;
        //check if heightNrElements is bigger than elements in the array
        if (this.heightNrElements > this.elementsArray.length) {
            this.heightNrElements = this.elementsArray.length;
        }
        this.multiple = multiple;
        this.curLowerIndex = offsetValue;
        //check if curLowIndex is within range
        if (this.curLowerIndex > (this.elementsArray.length - this.heightNrElements)) {
            this.curLowerIndex = this.elementsArray.length - this.heightNrElements;
        }
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
        this.functionToCall = functionToCall;
        
        //calculate other values
        this.triangleFourth = Math.round(this.cellHeight / 4); //this is only used internally
        this.textPadding = textPadding; //this is relative to the left of the cell
        this.textPaddingVertical = this.cellHeight - (this.cellHeight - this.textStyles["font-size"]) * 0.7; //this is relative to the top of the cell
        this.scrollerMinHeight = this.cellHeight * 0.5; //minimal height of the scroller rect

        //status variables
        this.scrollStep = 0; //y-value to go for one element
        this.scrollerHeight = 0; //height of dragable scroller bar
        this.scrollActive = false; //determines if scrolling per up/down button is active
        this.panY = false; //stores the y value of event
        this.scrollCumulus = 0; //if value is less then a scrollstep we need to accumulate scroll values
        this.scrollDir = ""; //later holds "up" and "down"
        this.exists = true; //true means it exists, gets value false if method "removeSelectionList" is called
        this.mouseDownStatus = false; //status that specifies if mouse is down or up on individual elements
        this.pressedKeys = ""; //stores key events (pressed char values)
        this.keyTimer = undefined;
        this.svgRootHasEventListener = false; //status variable to define if svg root has key and mouseover listener or not
    }
    else {
        createCombobox = false;
        alert("Error ("+id+"): wrong nr of arguments! You have to pass over "+nrArguments+" parameters.");
    }
    if (createCombobox) {
        //timer stuff
        this.timer = new Timer(this); //a Timer instance for calling the functionToCall
        this.timerMs = 200; //a constant of this object that is used in conjunction with the timer - functionToCall is called after 200 ms

        //create combobox
        var result = this.testParent();
        if (result) {
            //this.parentGroup = document.getElementById(this.id);
            this.createOrUpdateCombobox();
        }
        else {
            alert("could not create or reference 'parentNode' of combobox with id '"+this.id+"'");
        }            
    }
    else {
        alert("Could not create combobox with id '"+id+"' due to errors in the constructor parameters");    
    }    
}

//test if parent group exists or create a new group at the end of the file
combobox.prototype.testParent = function() {
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

combobox.prototype.createOrUpdateCombobox = function() {
    //check if there are more elements in the array than heightNrElements
    if (this.heightNrElements < this.elementsArray.length) {
        nrSelections = this.heightNrElements;
    }
    else {
        nrSelections = this.elementsArray.length;
    }
    //calculate height
    var comboboxHeight = this.cellHeight * nrSelections;
    //rectangle below (only fill, stroke is added at a separate rect)
    var created = true;
    if (!this.rectBelowBox) {
        this.rectBelowBox = document.createElementNS(svgNS,"rect");
        this.rectBelowBox.setAttributeNS(null,"stroke","none");
        created = false;
    }
    this.rectBelowBox.setAttributeNS(null,"x",this.xOffset);
    this.rectBelowBox.setAttributeNS(null,"y",this.yOffset);
    this.rectBelowBox.setAttributeNS(null,"width",this.width - this.cellHeight);
    this.rectBelowBox.setAttributeNS(null,"height",comboboxHeight);
    for (var attrib in this.boxStyles) {
        if (attrib != "stroke" && attrib != "stroke-width") {
            this.rectBelowBox.setAttributeNS(null,attrib,this.boxStyles[attrib]);
        }
    }        
    if (!created) {
        this.parentGroup.appendChild(this.rectBelowBox);
    }
    //create temporary group for indiv. combo elements
    var comboElementsGroupCreated = false;
    if (this.comboElementsGroup) {
        this.parentGroup.removeChild(this.comboElementsGroup);
        comboElementsGroupCreated = true;
    }
    this.comboElementsGroup = document.createElementNS(svgNS,"g");
    if (comboElementsGroupCreated) {
        this.parentGroup.insertBefore(this.comboElementsGroup,this.rectAboveBox);
    }
    else {
        this.parentGroup.appendChild(this.comboElementsGroup);        
    }

    //create rectangle above combobox (only stroke)
    created = true;
    if (!this.rectAboveBox) {
        this.rectAboveBox = document.createElementNS(svgNS,"rect");
        this.rectAboveBox.setAttributeNS(null,"fill","none");
        created = false;
    }
    this.rectAboveBox.setAttributeNS(null,"x",this.xOffset);
    this.rectAboveBox.setAttributeNS(null,"y",this.yOffset);
    this.rectAboveBox.setAttributeNS(null,"width",this.width - this.cellHeight);
    this.rectAboveBox.setAttributeNS(null,"height",comboboxHeight);
    for (var attrib in this.boxStyles) {
        if (attrib != "fill") {
            this.rectAboveBox.setAttributeNS(null,attrib,this.boxStyles[attrib]);
        }
    }        
    if (!created) {
        this.parentGroup.appendChild(this.rectAboveBox);
    }
    //build textElements from array
    //hold currentSelection Index to unroll list at new offset
    for (var i=0;i<nrSelections;i++) {
        //add rectangles to capture events
        var node = document.createElementNS(svgNS,"rect");
        node.setAttributeNS(null,"x",this.xOffset + this.textPadding / 2);
        node.setAttributeNS(null,"y",this.yOffset + this.cellHeight * i);
        node.setAttributeNS(null,"width",this.width - this.cellHeight - this.textPadding);
        node.setAttributeNS(null,"height",this.cellHeight);
        if (this.elementsArray[this.curLowerIndex + i].value) {
            for (var attrib in this.highlightStyles) {
                node.setAttributeNS(null,attrib,this.highlightStyles[attrib]);
            }        
        }
        else {
            node.setAttributeNS(null,"fill",this.boxStyles["fill"]);
        }
        node.setAttributeNS(null,"id","selHighlightSelection_" + this.id + "_" + (i + this.curLowerIndex));
        //add event-handler
        node.addEventListener("mousedown", this, false);
        node.addEventListener("mouseover", this, false);
        node.addEventListener("mouseup", this, false);
        this.comboElementsGroup.appendChild(node);
        //add text-elements
        var node = document.createElementNS(svgNS,"text");
        node.setAttributeNS(null,"id","selTexts_" + this.id + "_" + (i + this.curLowerIndex));
        node.setAttributeNS(null,"x",this.xOffset + this.textPadding);
        node.setAttributeNS(null,"y",this.yOffset + this.textPaddingVertical + this.cellHeight * i);
        node.setAttributeNS(null,"pointer-events","none");
        var value = "";
        for (var attrib in this.textStyles) {
            value = this.textStyles[attrib];
            if (attrib == "font-size") {
                value += "px";
            }
            node.setAttributeNS(null,attrib,value);
        }        
        var selectionText = document.createTextNode(this.elementsArray[this.curLowerIndex + i].key);
        node.appendChild(selectionText);
        this.comboElementsGroup.appendChild(node);
    }

    //create Scrollbar
    if (this.heightNrElements < this.elementsArray.length) {
        //calculate scrollstep
        this.scrollerHeight = (this.heightNrElements / this.elementsArray.length) * (comboboxHeight - 2 * this.cellHeight);
        if (this.scrollerHeight < this.scrollerMinHeight) {
            this.scrollerHeight = this.scrollerMinHeight;
        }
        this.scrollStep = (comboboxHeight - 2 * this.cellHeight - this.scrollerHeight) / (this.elementsArray.length - this.heightNrElements);
        //scrollbar
        created = true;
        if (!this.scrollbarRect) {
            this.scrollbarRect = document.createElementNS(svgNS,"rect");
            this.scrollbarRect.addEventListener("mousedown", this, false);
            this.scrollbarRect.setAttributeNS(null,"id","selScrollbarRect_"+this.id);
            created = false;
        }
        this.scrollbarRect.setAttributeNS(null,"x",this.xOffset + this.width - this.cellHeight);
        this.scrollbarRect.setAttributeNS(null,"y",this.yOffset + this.cellHeight);
        this.scrollbarRect.setAttributeNS(null,"width",this.cellHeight);
        this.scrollbarRect.setAttributeNS(null,"height",comboboxHeight - this.cellHeight * 2);
        for (var attrib in this.scrollbarStyles) {
            this.scrollbarRect.setAttributeNS(null,attrib,this.scrollbarStyles[attrib]);
        }        
        if (!created) {
            this.parentGroup.appendChild(this.scrollbarRect);
        }
        //upper rectangle
        created = true;
        if (!this.scrollUpperRect) {
            this.scrollUpperRect = document.createElementNS(svgNS,"rect");
            this.scrollUpperRect.setAttributeNS(null,"id","selScrollUpperRect_"+this.id);
            this.scrollUpperRect.addEventListener("mousedown", this, false);
            this.scrollUpperRect.addEventListener("mouseup", this, false);
            created = false;
        }
        this.scrollUpperRect.setAttributeNS(null,"x",this.xOffset + this.width - this.cellHeight);
        this.scrollUpperRect.setAttributeNS(null,"y",this.yOffset);
        this.scrollUpperRect.setAttributeNS(null,"width",this.cellHeight);
        this.scrollUpperRect.setAttributeNS(null,"height",this.cellHeight);
        for (var attrib in this.smallrectStyles) {
            this.scrollUpperRect.setAttributeNS(null,attrib,this.smallrectStyles[attrib]);
        }        
        if (!created) {
            this.parentGroup.appendChild(this.scrollUpperRect);
        }
        //upper triangle
        created = true;
        if (!this.upperTriangle) {
            this.upperTriangle = document.createElementNS(svgNS,"path");
            this.upperTriangle.setAttributeNS(null,"pointer-events","none");
            created = false;
        }
        var myPath = "M"+(this.xOffset + this.width - 3 * this.triangleFourth)+" "+(this.yOffset + 3 * this.triangleFourth)+" L"+(this.xOffset + this.width - this.triangleFourth)+" "+(this.yOffset + 3 * this.triangleFourth)+" L"+(this.xOffset + this.width - 2 * this.triangleFourth)+" "+(this.yOffset + this.triangleFourth)+" Z";
        this.upperTriangle.setAttributeNS(null,"d",myPath);
        for (var attrib in this.triangleStyles) {
            this.upperTriangle.setAttributeNS(null,attrib,this.triangleStyles[attrib]);
        }        
        if (!this.created) {
            this.parentGroup.appendChild(this.upperTriangle);
        }
        //lower rectangle
        created = true;
        if (!this.scrollLowerRect) {
            this.scrollLowerRect = document.createElementNS(svgNS,"rect");
            this.scrollLowerRect.setAttributeNS(null,"id","selScrollLowerRect_" + this.id);
            this.scrollLowerRect.addEventListener("mousedown", this, false);
            this.scrollLowerRect.addEventListener("mouseup", this, false);
            created = false;
        }
        this.scrollLowerRect.setAttributeNS(null,"x",this.xOffset + this.width - this.cellHeight);
        this.scrollLowerRect.setAttributeNS(null,"y",this.yOffset + comboboxHeight - this.cellHeight);
        this.scrollLowerRect.setAttributeNS(null,"width",this.cellHeight);
        this.scrollLowerRect.setAttributeNS(null,"height",this.cellHeight);
        for (var attrib in this.smallrectStyles) {
            this.scrollLowerRect.setAttributeNS(null,attrib,this.smallrectStyles[attrib]);
        }        
        if (!created) {
            this.parentGroup.appendChild(this.scrollLowerRect);
        }
        //lower triangle
        created = true;
        if (!this.lowerTriangle) {
            this.lowerTriangle = document.createElementNS(svgNS,"path");
            this.lowerTriangle.setAttributeNS(null,"pointer-events","none");
            created = false;
        }
        var myPath = "M"+(this.xOffset + this.width - 3 * this.triangleFourth)+" "+(this.yOffset + comboboxHeight - this.cellHeight + this.triangleFourth)+" L"+(this.xOffset + this.width - this.triangleFourth)+" "+(this.yOffset + comboboxHeight - this.cellHeight + this.triangleFourth)+" L"+(this.xOffset + this.width - 2 * this.triangleFourth)+" "+(this.yOffset + comboboxHeight - this.cellHeight + 3 * this.triangleFourth)+" Z";
        this.lowerTriangle.setAttributeNS(null,"d",myPath);
        for (var attrib in this.triangleStyles) {
            this.lowerTriangle.setAttributeNS(null,attrib,this.triangleStyles[attrib]);
        }        
        if (!created) {
            this.parentGroup.appendChild(this.lowerTriangle);
        }
        //scrollerRect
        created = true;
        if (!this.scroller) {
            this.scroller = document.createElementNS(svgNS,"rect");
            this.scroller.setAttributeNS(null,"pointer-events","none");
            this.scroller.setAttributeNS(null,"id","selScroller_"+this.id);
            created = false;
        }
        this.scroller.setAttributeNS(null,"x",this.xOffset + this.width - this.cellHeight);
        this.scroller.setAttributeNS(null,"y",this.yOffset + this.cellHeight + this.scrollStep * this.curLowerIndex);
        this.scroller.setAttributeNS(null,"width",this.cellHeight);
        this.scroller.setAttributeNS(null,"height",this.scrollerHeight);
        for (var attrib in this.smallrectStyles) {
            this.scroller.setAttributeNS(null,attrib,this.smallrectStyles[attrib]);
        }        
        if (!created) {
            this.parentGroup.appendChild(this.scroller);
        }
    }
}

combobox.prototype.handleEvent = function(evt) {
    var el = evt.currentTarget;
    var callerId = el.getAttributeNS(null,"id");
    var myRegExp = new RegExp(this.id);
    if (evt.type == "mouseover") {
        if (myRegExp.test(callerId)) {
            if (callerId.match(/\bselHighlightSelection_/)) {
                if (this.mouseDownStatus) {
                    this.selectOrUnselect(evt);
                    evt.stopPropagation();
                }
                else {
                    if (!this.svgRootHasEventListener) {
                        document.documentElement.addEventListener("mouseover",this,false);
                        document.documentElement.addEventListener("keypress",this,false);
                        this.svgRootHasEventListener = true;
                    }
                }
            }
        }
        if (!myRegExp.test(evt.target.getAttributeNS(null,"id"))) {
            //case that the event comes from the documentRoot element
            //but not from the combobox itself
            this.mouseDownStatus = false;
            document.documentElement.removeEventListener("mouseover",this,false);
            document.documentElement.removeEventListener("keypress",this,false);
            this.svgRootHasEventListener = false;
        }
    }
    if (evt.type == "mousedown") {
        if (myRegExp.test(callerId)) {
            if (callerId.match(/\bselHighlightSelection_/)) {
                this.selectOrUnselect(evt);
                this.mouseDownStatus = true;
                document.documentElement.addEventListener("mouseup",this,false);
                evt.stopPropagation();
            }
        }
        if (callerId.match(/\bselScrollUpperRect_|\bselScrollLowerRect/)) {
            this.scrollDir = "down";
            this.scrollActive = true;
            document.documentElement.addEventListener("mouseup",this,false);
            if (callerId.match(/UpperRect/)) {
                this.scrollDir = "up";
            }
            this.scroll(this.scrollDir,1,true); //1 is one element to scroll, true says that we should move scrollbar
            this.timer.setTimeout("scrollPerButton",400)
        }
        if (callerId.match(/\bselScrollbarRect_/)) {
            this.scrollBarMove(evt);
        }
    }
    if (evt.type == "mouseup") {
        if (myRegExp.test(callerId)) {
            if (callerId.match(/\bselHighlightSelection_/)) {
                this.mouseDownStatus = false;
                if (this.panStatus) {
                    this.scrollBarMove(evt);                    
                }
                if (this.scrollActive) {
                    this.scrollActive = false;
                    document.documentElement.removeEventListener("mouseup",this,false);
                }
                evt.stopPropagation();
            }
            if (callerId.match(/\bselScrollUpperRect_|\bselScrollLowerRect/)) {
                this.scrollActive = false;
            }
        }
        if (el.nodeName == "svg") {
            if (this.panStatus) {
                //stop scroll mode
                this.scrollBarMove(evt);                
            }
            if (this.scrollActive) {
                this.scrollActive = false;
                document.documentElement.removeEventListener("mouseup",this,false);                
            }
        }
    }
    if (evt.type == "mousemove") {
        if (el.nodeName == "svg") {
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

combobox.prototype.moveTo = function(moveX,moveY) {
    this.xOffset = moveX;
    this.yOffset = moveY;
    this.createOrUpdateCombobox();
}

combobox.prototype.resize = function(newWidth) {
    this.width = newWidth;
    this.createOrUpdateCombobox();
}

combobox.prototype.resetPressedKeys = function() {
     this.keyTimer = undefined;
     this.pressedKeys = "";
}

combobox.prototype.selectOrUnselect = function(evt) {
    var mySelEl = evt.target;
    var result = mySelEl.getAttributeNS(null,"id").split("_");
    var idNr = result[2]; //the numerical id in the array
    if (this.multiple) {
        //multiple selections are allowed
        if (evt.type == "mousedown") {
            if (evt.ctrlKey) {
                if (this.elementsArray[idNr].value) {
                    mySelEl.setAttributeNS(null,"fill",this.boxStyles["fill"]);
                    if (mySelEl.hasAttributeNS(null,"fill-opacity")) {
                        mySelEl.removeAttributeNS(null,"fill-opacity");
                    }
                    this.elementsArray[idNr].value = false;
                }
                else {
                    for (var attrib in this.highlightStyles) {
                        mySelEl.setAttributeNS(null,attrib,this.highlightStyles[attrib]);
                    }        
                    this.elementsArray[idNr].value = true;
                }
            }
            else {
                if (evt.shiftKey) {
                    var selectedArray = new Array();
                    for (var i=0;i<this.elementsArray.length;i++) {
                        //see how many and where elements are already selected
                        if (this.elementsArray[i].value) {
                            selectedArray.push(i);
                        }
                    }
                    if (selectedArray.length == 0) {
                        //if no element was selected we select the given element
                        for (var attrib in this.highlightStyles) {
                            mySelEl.setAttributeNS(null,attrib,this.highlightStyles[attrib]);
                        }        
                        this.elementsArray[idNr].value = true;
                    }
                    else {
                        //now we need to calculate the distances to other
                        var distance = this.elementsArray.length;
                        var nearestElement = 0;
                        for (var i=0;i<selectedArray.length;i++) {
                            var tempDist = Math.abs(idNr - selectedArray[i]);
                            if (tempDist < distance) {
                                distance = tempDist;
                                nearestElement = selectedArray[i];
                            }
                        }
                        if (nearestElement < idNr) {
                            var lowerIndex = nearestElement;
                            var upperIndex = idNr;
                        }
                        else {
                            var lowerIndex = idNr;
                            var upperIndex = nearestElement;
                        }
                        for (var i=lowerIndex;i<=upperIndex;i++) {
                            var mySelEl = document.getElementById("selHighlightSelection_" + this.id + "_" + i);
                            for (var attrib in this.highlightStyles) {
                                mySelEl.setAttributeNS(null,attrib,this.highlightStyles[attrib]);
                            }        
                            this.elementsArray[i].value = true;
                        }
                    }
                }
                else {
                    this.deselectAll(false);
                    for (var attrib in this.highlightStyles) {
                        mySelEl.setAttributeNS(null,attrib,this.highlightStyles[attrib]);
                    }        
                    this.elementsArray[idNr].value = true;
                }
            }
        }
        if (evt.type == "mouseover") {
            for (var attrib in this.highlightStyles) {
                mySelEl.setAttributeNS(null,attrib,this.highlightStyles[attrib]);
            }        
            this.elementsArray[idNr].value = true;
        }
    }
    else {
        //only one selection is allowed at the time
        this.deselectAll(false);
        for (var attrib in this.highlightStyles) {
            mySelEl.setAttributeNS(null,attrib,this.highlightStyles[attrib]);
        }        
        this.elementsArray[idNr].value = true;
    }
    if (this.functionToCall) {
        this.timer.setTimeout("fireFunction",this.timerMs);
    }
}

combobox.prototype.deselectAll = function(fireFunction) {
    for (var i=0;i<this.elementsArray.length;i++) {
        if (this.elementsArray[i].value) {
            this.elementsArray[i].value = false;
            if (i >= this.curLowerIndex && i < (this.curLowerIndex + this.heightNrElements)) {
                var mySelEl = document.getElementById("selHighlightSelection_" + this.id + "_" + i);
                mySelEl.setAttributeNS(null,"fill",this.boxStyles["fill"]);
                if (mySelEl.hasAttributeNS(null,"fill-opacity")) {
                    mySelEl.removeAttributeNS(null,"fill-opacity");
                }
            }
        }
    }
    if (fireFunction) {
        this.timer.setTimeout("fireFunction",this.timerMs);
    }
}

combobox.prototype.selectAll = function(fireFunction) {
    for (var i=0;i<this.elementsArray.length;i++) {
        if (!this.elementsArray[i].value) {
            this.elementsArray[i].value = true;
            if (i >= this.curLowerIndex && i < (this.curLowerIndex + this.heightNrElements)) {
                var mySelEl = document.getElementById("selHighlightSelection_" + this.id + "_" + i);
                for (var attrib in this.highlightStyles) {
                    mySelEl.setAttributeNS(null,attrib,this.highlightStyles[attrib]);
                }        
            }
        }
    }
    if (fireFunction) {
        this.timer.setTimeout("fireFunction",this.timerMs);
    }
}

combobox.prototype.invertSelection = function(fireFunction) {
    for (var i=0;i<this.elementsArray.length;i++) {
        if (this.elementsArray[i].value) {
            this.elementsArray[i].value = false;
            if (i >= this.curLowerIndex && i < (this.curLowerIndex + this.heightNrElements)) {
                var mySelEl = document.getElementById("selHighlightSelection_" + this.id + "_" + i);
                mySelEl.setAttributeNS(null,"fill",this.boxStyles["fill"]);
                if (mySelEl.hasAttributeNS(null,"fill-opacity")) {
                    mySelEl.removeAttributeNS(null,"fill-opacity");
                }
            }
        }
        else {
            this.elementsArray[i].value = true;
            if (i >= this.curLowerIndex && i < (this.curLowerIndex + this.heightNrElements)) {
                var mySelEl = document.getElementById("selHighlightSelection_" + this.id + "_" + i);
                for (var attrib in this.highlightStyles) {
                    mySelEl.setAttributeNS(null,attrib,this.highlightStyles[attrib]);
                }        
            }
        }
    }
    if (fireFunction) {
        this.timer.setTimeout("fireFunction",this.timerMs);
    }
}

combobox.prototype.fireFunction = function() {
    var selectedArray = new Array();
    var selectedArrayIndizes = new Array();
    for (var i=0;i<this.elementsArray.length;i++) {
        //see how many and where elements are already selected
        if (this.elementsArray[i].value) {
            selectedArray.push(this.elementsArray[i].key);
            selectedArrayIndizes.push(i);
        }
    }
    if (typeof(this.functionToCall) == "function") {
        this.functionToCall(this.id,selectedArray,selectedArrayIndizes);
    }
    if (typeof(this.functionToCall) == "object") {
        this.functionToCall.getComboboxVals(this.id,selectedArray,selectedArrayIndizes);
    }
    if (typeof(this.functionToCall) == undefined) {
        return;
    }
}

combobox.prototype.scrollPerButton = function() {
    if (this.scrollActive == true) {
        this.scroll(this.scrollDir,1,true); //1 is one element to scroll, true says that we should move scrollbar
        this.timer.setTimeout("scrollPerButton",100)
    }
}

combobox.prototype.scroll = function(scrollDir,scrollNr,scrollBool) {
    if (scrollNr < this.heightNrElements) {
        if ((this.curLowerIndex > 0) && (scrollDir == "up")) {
            if (scrollNr > this.curLowerIndex) {
                scrollNr = this.curLowerIndex;
            }
            //decrement current index
            this.curLowerIndex = this.curLowerIndex - scrollNr;
            //move scroller
            if (scrollBool == true) {
                this.scroller.setAttributeNS(null,"y",parseFloat(this.scroller.getAttributeNS(null,"y"))+ this.scrollStep * -1);
            }
            //add upper rect elements
            for (var i=0;i<scrollNr;i++) {
                var node = document.createElementNS(svgNS,"rect");
                node.setAttributeNS(null,"x",this.xOffset + this.textPadding / 2);
                node.setAttributeNS(null,"y",this.yOffset + this.cellHeight * i);
                node.setAttributeNS(null,"width",this.width - this.cellHeight - this.textPadding);
                node.setAttributeNS(null,"height",this.cellHeight);
                if (this.elementsArray[this.curLowerIndex + i].value) {
                    for (var attrib in this.highlightStyles) {
                        node.setAttributeNS(null,attrib,this.highlightStyles[attrib]);
                    }        
                }
                else {
                    node.setAttributeNS(null,"fill",this.boxStyles["fill"]);
                    if (node.hasAttributeNS(null,"fill-opacity")) {
                        node.removeAttributeNS(null,"fill-opacity");
                    }
                }
                node.setAttributeNS(null,"id","selHighlightSelection_" + this.id + "_" + (i + this.curLowerIndex));
                //add event-handler
                node.addEventListener("mousedown", this, false);
                node.addEventListener("mouseover", this, false);
                node.addEventListener("mouseup", this, false);
                this.comboElementsGroup.appendChild(node);
                //add text-nodes
                var node = document.createElementNS(svgNS,"text");
                node.setAttributeNS(null,"id","selTexts_" + this.id + "_" + (i + this.curLowerIndex));
                node.setAttributeNS(null,"x",this.xOffset + this.textPadding);
                node.setAttributeNS(null,"y",this.yOffset + this.textPaddingVertical + this.cellHeight * i);
                node.setAttributeNS(null,"pointer-events","none");
                var value = "";
                for (var attrib in this.textStyles) {
                    value = this.textStyles[attrib];
                    if (attrib == "font-size") {
                        value += "px";
                    }
                    node.setAttributeNS(null,attrib,value);
                }        
                var selectionText = document.createTextNode(this.elementsArray[this.curLowerIndex + i].key);
                node.appendChild(selectionText);
                this.comboElementsGroup.appendChild(node);
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
                this.comboElementsGroup.removeChild(node);
                var node = document.getElementById("selHighlightSelection_" + this.id +"_" + (k + this.curLowerIndex));
                this.comboElementsGroup.removeChild(node);
            }
        }
        else if ((this.curLowerIndex < (this.elementsArray.length - this.heightNrElements)) && (scrollDir == "down")) {
            //move Scroller
            if (scrollBool == true) {
                this.scroller.setAttributeNS(null,"y",parseFloat(this.scroller.getAttributeNS(null,"y")) + this.scrollStep);
            }
            //remove most upper element
            for (var i=0;i<scrollNr;i++) {
                var node = document.getElementById("selTexts_" + this.id + "_" + (this.curLowerIndex + i));
                this.comboElementsGroup.removeChild(node);
                var node = document.getElementById("selHighlightSelection_" + this.id + "_" + (this.curLowerIndex + i));
                this.comboElementsGroup.removeChild(node);
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
                node.setAttributeNS(null,"y",this.yOffset + this.cellHeight * (k - scrollNr));
                node.setAttributeNS(null,"width",this.width - this.cellHeight - this.textPadding);
                node.setAttributeNS(null,"height",this.cellHeight);
                if (this.elementsArray[this.curLowerIndex + k].value) {
                    for (var attrib in this.highlightStyles) {
                        node.setAttributeNS(null,attrib,this.highlightStyles[attrib]);
                    }        
                }
                else {
                    node.setAttributeNS(null,"fill",this.boxStyles["fill"]);
                    if (node.hasAttributeNS(null,"fill-opacity")) {
                        node.removeAttributeNS(null,"fill-opacity");
                    }
                }
                node.setAttribute("id","selHighlightSelection_" + this.id + "_" + (k + this.curLowerIndex));
                //add event-handler
                node.addEventListener("mousedown", this, false);
                node.addEventListener("mouseover", this, false);
                node.addEventListener("mouseup", this, false);
                this.comboElementsGroup.appendChild(node);
                //add text-nodes
                var node = document.createElementNS(svgNS,"text");
                node.setAttributeNS(null,"id","selTexts_" + this.id + "_" + (k + this.curLowerIndex));
                node.setAttributeNS(null,"x",this.xOffset + this.textPadding);
                node.setAttributeNS(null,"y",this.yOffset + this.textPaddingVertical + this.cellHeight * (k - scrollNr));
                node.setAttributeNS(null,"pointer-events","none");
                var value = "";
                for (var attrib in this.textStyles) {
                    value = this.textStyles[attrib];
                    if (attrib == "font-size") {
                        value += "px";
                    }
                    node.setAttributeNS(null,attrib,value);
                }        
                var selectionText = document.createTextNode(this.elementsArray[this.curLowerIndex + k].key);
                node.appendChild(selectionText);
                this.comboElementsGroup.appendChild(node);
            }
            //increment current index
            this.curLowerIndex = this.curLowerIndex + scrollNr;
        }
    }
    else {
            //remove lower elements
            for (var i=0;i<this.heightNrElements;i++) {
                var node = document.getElementById("selTexts_" + this.id + "_" + (i + this.curLowerIndex));
                this.comboElementsGroup.removeChild(node);
                var node = document.getElementById("selHighlightSelection_" + this.id +"_" + (i + this.curLowerIndex));
                this.comboElementsGroup.removeChild(node);
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
                node.setAttributeNS(null,"y",this.yOffset + this.cellHeight * i);
                node.setAttributeNS(null,"width",this.width - this.cellHeight - this.textPadding);
                node.setAttributeNS(null,"height",this.cellHeight);
                if (this.elementsArray[this.curLowerIndex + i].value) {
                    for (var attrib in this.highlightStyles) {
                        node.setAttributeNS(null,attrib,this.highlightStyles[attrib]);
                    }
                }
                else {
                    node.setAttributeNS(null,"fill",this.boxStyles["fill"]);
                    if (node.hasAttributeNS(null,"fill-opacity")) {
                        node.removeAttributeNS(null,"fill-opacity");
                    }
                }
                node.setAttributeNS(null,"id","selHighlightSelection_" + this.id + "_" + (i + this.curLowerIndex));
                //add event-handler
                node.addEventListener("mousedown", this, false);
                node.addEventListener("mouseover", this, false);
                node.addEventListener("mouseup", this, false);
                this.comboElementsGroup.appendChild(node);
                //add text-nodes
                var node = document.createElementNS(svgNS,"text");
                node.setAttributeNS(null,"id","selTexts_" + this.id + "_" + (i + this.curLowerIndex));
                node.setAttributeNS(null,"x",this.xOffset + this.textPadding);
                node.setAttributeNS(null,"y",this.yOffset + this.textPaddingVertical + this.cellHeight * i);
                node.setAttributeNS(null,"pointer-events","none");
                var value = "";
                for (var attrib in this.textStyles) {
                    value = this.textStyles[attrib];
                    if (attrib == "font-size") {
                        value += "px";
                    }
                    node.setAttributeNS(null,attrib,value);
                }        
                var selectionText = document.createTextNode(this.elementsArray[this.curLowerIndex + i].key);
                node.appendChild(selectionText);
                this.comboElementsGroup.appendChild(node);
            }
    }
}

//event listener for Scrollbar element
combobox.prototype.scrollBarMove = function(evt) {
    var scrollerMinY = parseFloat(this.scrollbarRect.getAttributeNS(null,"y"));
    var scrollerMaxY = parseFloat(this.scrollbarRect.getAttributeNS(null,"y")) + parseFloat(this.scrollbarRect.getAttributeNS(null,"height")) - parseFloat(this.scroller.getAttributeNS(null,"height"));
    if (evt.type == "mousedown") {
        this.panStatus = true;
        //add event listeners to root element
        var svgroot = document.documentElement;
        svgroot.addEventListener("mousemove", this, false);
        svgroot.addEventListener("mouseup", this, false);
        for (var attrib in this.triangleStyles) {
            this.scroller.setAttributeNS(null,attrib,this.triangleStyles[attrib]);
        }        
        var coordPoint = myMapApp.calcCoord(evt,this.parentGroup);
        this.panY = coordPoint.y;
        var oldY = parseFloat(this.scroller.getAttributeNS(null,"y"));
        var newY = this.panY - parseFloat(this.scroller.getAttributeNS(null,"height")) / 2;
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
            this.scroller.setAttributeNS(null,"y",newY);
            this.scroll(scrollDir,scrollNr,false);
        }
    }
    if (evt.type == "mouseup") {
        if (this.panStatus == true) {
            var newY = parseFloat(this.scroller.getAttributeNS(null,"y"));
            for (var attrib in this.smallrectStyles) {
                this.scroller.setAttributeNS(null,attrib,this.smallrectStyles[attrib]);
            }        
            this.scroller.setAttributeNS(null,"y",this.yOffset + this.cellHeight + this.scrollStep * this.curLowerIndex);
        }
        var svgroot = document.documentElement;
        svgroot.removeEventListener("mousemove", this, false);
        svgroot.removeEventListener("mouseup", this, false);
        this.panStatus = false;
    }
    if (evt.type == "mousemove") {
        if (this.panStatus == true) {
            var coordPoint = myMapApp.calcCoord(evt,this.parentGroup);
            var panNewEvtY = coordPoint.y;
            var panDiffY = panNewEvtY - this.panY;
            var oldY = parseFloat(this.scroller.getAttributeNS(null,"y"));
            var newY = oldY + panDiffY;
            if (newY < scrollerMinY) {
                newY = scrollerMinY;
            }
            if (newY > scrollerMaxY) {
                newY = scrollerMaxY;
            }
            var panDiffY = newY - oldY;
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
                    else if (newY == scrollerMaxY && this.curLowerIndex != (this.elementsArray.length -     this.heightNrElements)) {
                        this.scroll("down",1,false);
                    }
                }
            }
            newY = oldY + panDiffY;
            this.scroller.setAttributeNS(null,"y",newY);
            this.panY = panNewEvtY;
        }
    }
}

combobox.prototype.scrollToKey = function() {
    for (var i=0;i<this.elementsArray.length;i++) {
        if (this.elementsArray[i].key.toLowerCase().substr(0,this.pressedKeys.length) == this.pressedKeys) {
            this.curLowerIndex = i;
            if (this.curLowerIndex > (this.elementsArray.length - this.heightNrElements)) {
                this.curLowerIndex = this.elementsArray.length - this.heightNrElements;
            }
            this.createOrUpdateCombobox();
            break;
        }
    }
}

combobox.prototype.elementExists = function(elementName) {
    var result = -1;
    for (i=0;i<this.elementsArray.length;i++) {
        if (this.elementsArray[i].key == elementName) {
            result = i;
        }
    }
    return result;
}

combobox.prototype.selectElementsByName = function(elementNames,fireFunction) {
    var idsToSelect = new Array();
    for (var i=0;i<elementNames.length;i++) {
        var indexNr = this.elementExists(elementNames[i].key);
        if (indexNr != -1) {
            idsToSelect.push({index:indexNr,value:elementNames[i].value});
        }
    }
    if (idsToSelect.length > 0) {
        this.selectElementsByPosition(idsToSelect,fireFunction);
    }
}

combobox.prototype.selectElementsByPosition = function(positionArray,fireFunction) {
    for (var i=0;i<positionArray.length;i++) {
        if (positionArray[i].index < this.elementsArray.length) {
            if (positionArray[i].index >= this.curLowerIndex && positionArray[i].index < (this.curLowerIndex + this.heightNrElements)) {
                var node = document.getElementById("selHighlightSelection_" + this.id + "_" + positionArray[i].index);
                if (positionArray[i].value) {
                    for (var attrib in this.highlightStyles) {
                        node.setAttributeNS(null,attrib,this.highlightStyles[attrib]);
                    }        
                }
                else {
                    node.setAttributeNS(null,"fill",this.boxStyles["fill"]);
                    if (node.hasAttributeNS(null,"fill-opacity")) {
                        node.removeAttributeNS(null,"fill-opacity");
                    }
                }
            }
            this.elementsArray[positionArray[i].index].value = positionArray[i].value;
        }
    }
    if (fireFunction) {
        this.timer.setTimeout("fireFunction",this.timerMs);
    }
}

combobox.prototype.sortList = function(direction) {
    //direction: asc|desc, for ascending or descending
    var mySelElementString = this.elementsArray[this.activeSelection];
    this.elementsArray.sort(function mySort(a,b) {
        if (a.key > b.key) {
            return 1;
        }
        else {
            return -1;
        }
    });
    if (direction == "desc") {
        this.elementsArray.reverse();
    }
    this.createOrUpdateCombobox();
}

combobox.prototype.deleteElementsByName = function(elementNames,fireFunction) {
    for (var i=0;i<elementNames.length;i++) {
        existsPosition = this.elementExists(elementNames[i]);
        if (existsPosition != -1) {
            var tempArray = new Array;
            tempArray = tempArray.concat(this.elementsArray.slice(0,existsPosition),this.elementsArray.slice(existsPosition + 1,this.elementsArray.length));
            this.elementsArray = tempArray;
            if (existsPosition < this.curLowerIndex) {
                this.curLowerIndex--;
            }
        }
    }
    this.createOrUpdateCombobox();
    if (fireFunction) {
        this.timer.setTimeout("fireFunction",this.timerMs);
    }
}

combobox.prototype.deleteElementByPosition = function(elementPosition,fireFunction) {
    if (elementPosition < this.elementsArray.length) {
        var tempArray = new Array;
        tempArray = tempArray.concat(this.elementsArray.slice(0,elementPosition),this.elementsArray.slice(elementPosition + 1,this.elementsArray.length));
        this.elementsArray = tempArray;
        if (elementPosition < this.curLowerIndex) {
            this.curLowerIndex -= 1;
        }
    }
    this.createOrUpdateCombobox();
    if (fireFunction) {
        this.timer.setTimeout("fireFunction",this.timerMs);
    }
}

combobox.prototype.addElementAtPosition = function(element,position,fireFunction) {
    if (position > this.elementsArray.length) {
        this.elementsArray.push(element);
        position = this.elementsArray.length - 1;
    }
    else {
        var tempArray = new Array;
        tempArray = tempArray.concat(this.elementsArray.slice(0,position),element,this.elementsArray.slice(position,this.elementsArray.length));
        this.elementsArray = tempArray;
    }
    if (position < this.curLowerIndex) {
        this.curLowerIndex += 1;
    }
    this.createOrUpdateCombobox();
    if (fireFunction) {
        this.timer.setTimeout("fireFunction",this.timerMs);
    }
}

combobox.prototype.addElementsAlphabetically = function(elementsArr,direction,fireFunction) {
    this.elementsArray = this.elementsArray.concat(elementsArr);
    this.sortList(direction);
    if (fireFunction) {
        this.timer.setTimeout("fireFunction",this.timerMs);
    }
}

combobox.prototype.getCurrentSelections = function() {
    var selectedArray = new Array();
    for (var i=0;i<this.elementsArray.length;i++) {
        //see how many and where elements are already selected
        if (this.elementsArray[i].value) {
            selectedArray.push(this.elementsArray[i].key);
        }
    }
    return selectedArray;
}

combobox.prototype.getCurrentSelectionsIndex = function() {
    var selectedArrayIndizes = new Array();
    for (var i=0;i<this.elementsArray.length;i++) {
        //see how many and where elements are already selected
        if (this.elementsArray[i].value) {
            selectedArrayIndizes.push(i);
        }
    }
    return selectedArrayIndizes;
}

combobox.prototype.removeCombobox = function() {
    //remove all Elements of combobox
    this.exists = false;
    while (this.parentGroup.hasChildNodes()) {
        this.parentGroup.removeChild(this.parentGroup.firstChild);
    }
}
