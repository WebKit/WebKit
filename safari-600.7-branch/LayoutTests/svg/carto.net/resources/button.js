/*
Scripts to create interactive buttons in SVG using ECMA script
Copyright (C) <2006>  <Andreas Neumann>
Version 1.1.3, 2006-10-30
neumann@karto.baug.ethz.ch
http://www.carto.net/
http://www.carto.net/neumann/

Credits:
* Bruce Rindahl and Virgis Globis for fixing an error with vertical positioning of the button texts
* Olaf Schnabel for suggesting a bugfix where type conversion was missing when button text was of type number

----

Documentation: http://www.carto.net/papers/svg/gui/button/

----

current version: 1.1.3

version history:
1.0 (2006-02-08)
initial version

1.0.1 (2006-03-11)
changed parameters of constructor (styling system): now an array of literals containing presentation attributes. This allows for more flexibility in Styling. Added check for number of arguments.

1.0.2 (2006-07-07)
added the option to create multiline textbuttons, changed the behaviour of the y placement of the text in the button

1.1 (2006-07-12)
added .resize(width,height) and .moveTo(x.y) methods, added new constructor parameter parentNode, fixed a bug from version 1.0.2 in the method .setTextValue()

1.1.1 (2006-10-23)
fixed an error with vertical positioning of button texts (thanks Bruce and Virgis)
changed DOM hierarchy slightly. Every button now has its own group "this.parentGroup". This is necessary to hide and show a button. This group should not be shared by other SVG elements, unless on purpose.
adopted .resize() and .moveTo() methods to make use of the corrected function this.createButtonTexts();

1.1.2 (2006-10-26)
corrected a bug where type conversion in buttonText was missing when buttonText was of type number

1.1.3 (2006-10-30)
corrected a bug where the .deactivate() method did not work properly when called after the method .setTextValue()
added support for "rx" and "ry" attribute in "this.deActivateRect"

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

original document site: http://www.carto.net/papers/svg/gui/button/
Please contact the author in case you want to use code or ideas commercially.
If you use this code, please include this copyright header, the included full
LGPL 2.1 text and read the terms provided in the LGPL 2.1 license
(http://www.gnu.org/copyleft/lesser.txt)

-------------------------------

Please report bugs and send improvements to neumann@karto.baug.ethz.ch
If you use this control, please link to the original (http://www.carto.net/papers/svg/gui/button/)
somewhere in the source-code-comment or the "about" of your project and give credits, thanks!

*/

function button(id,parentNode,functionToCall,buttonType,buttonText,buttonSymbolId,x,y,width,height,textStyles,buttonStyles,shadeLightStyles,shadeDarkStyles,shadowOffset) {    
    var nrArguments = 15;
    if (arguments.length > 0) {
        if (arguments.length == nrArguments) {
            this.init(id,parentNode,functionToCall,buttonType,buttonText,buttonSymbolId,x,y,width,height,textStyles,buttonStyles,shadeLightStyles,shadeDarkStyles,shadowOffset);
        }
        else {
            alert("Error in Button ("+id+"): wrong nr of arguments! You have to pass over "+nrArguments+" parameters.");        
        }
    }
}

button.prototype.init = function(id,parentNode,functionToCall,buttonType,buttonText,buttonSymbolId,x,y,width,height,textStyles,buttonStyles,shadeLightStyles,shadeDarkStyles,shadowOffset) {
    this.id = id; //the id where all new content is appended to
    this.parentNode = parentNode; //the id or node reference of the parent group where the button can be appended
    this.functionToCall = functionToCall; //function to be called if button was pressed
    this.buttonType = buttonType; //button type: currently either "rect" or "ellipse"
    this.buttonText = buttonText; //default value to be filled in when button is created
    this.buttonSymbolId = buttonSymbolId; //id to a symbol to be used as a button graphics
    this.x = x; //left of button rectangle
    this.y = y; //top of button rectangle
    this.width = width; //button rectangle width
    this.height = height; //button rectangle height
    this.textStyles = textStyles; //array of literals containing text styles
    if (!this.textStyles["font-size"]) {
        this.textStyles["font-size"] = 12;
    }    
    this.buttonStyles = buttonStyles; //the fill color of the button rectangle or ellipse
    this.shadeLightStyles = shadeLightStyles; //light fill color simulating 3d effect
    this.shadeDarkStyles = shadeDarkStyles; //dark fill color simulating 3d effect
    this.shadowOffset = shadowOffset; //shadow offset in viewBox units
    this.upperLeftLine = null; //later a reference to the upper left line simulating 3d effect
    this.buttonRect = null; //later a reference to the button area (rect)
    this.buttonTextElement = null; //later a reference to the button text
    this.buttonSymbolInstance = null; //later a reference to the button symbol
    this.deActivateRect = null; //later a reference to a rectangle that can be used to deactivate the button
    this.activated = true; //a property indicating if button is activated or not
    this.lowerRightLine = null; //later a reference to the lower right line simulating 3d effect
    this.createButton(); //method to initialize button
    this.timer = new Timer(this); //a Timer instance for calling the functionToCall
    this.timerMs = 200; //a constant of this object that is used in conjunction with the timer - functionToCall is called after 200 ms
}

//create button
button.prototype.createButton = function() {
    var result = this.testParent();
        if (result) {
            //create upper left button line or ellipse
            if (this.buttonType == "rect") {
                this.upperLeftShadow = document.createElementNS(svgNS,"rect");
                this.upperLeftShadow.setAttributeNS(null,"x",this.x - this.shadowOffset);
                this.upperLeftShadow.setAttributeNS(null,"y",this.y - this.shadowOffset);
                this.upperLeftShadow.setAttributeNS(null,"width",this.width);
                this.upperLeftShadow.setAttributeNS(null,"height",this.height);
                this.upperLeftShadow.setAttributeNS(null,"points",this.x+","+(this.y+this.height)+" "+this.x+","+this.y+" "+(this.x+this.width)+","+this.y);
            }
            else if (this.buttonType == "ellipse") {
                this.upperLeftShadow = document.createElementNS(svgNS,"ellipse");
                this.upperLeftShadow.setAttributeNS(null,"cx",this.x + this.width * 0.5 - this.shadowOffset);
                this.upperLeftShadow.setAttributeNS(null,"cy",this.y + this.height * 0.5 - this.shadowOffset);
                this.upperLeftShadow.setAttributeNS(null,"rx",this.width * 0.5);
                this.upperLeftShadow.setAttributeNS(null,"ry",this.height * 0.5);
            }
            else {
                alert("buttonType '"+this.buttonType+"' not supported. You need to specify 'rect' or 'ellipse'");
            }
            for (var attrib in this.shadeLightStyles) {
                this.upperLeftShadow.setAttributeNS(null,attrib,this.shadeLightStyles[attrib]);
            }
            this.parentGroup.appendChild(this.upperLeftShadow);
            
            //create lower right button line or ellipse
            if (this.buttonType == "rect") {
                this.lowerRightShadow = document.createElementNS(svgNS,"rect");
                this.lowerRightShadow.setAttributeNS(null,"x",this.x + this.shadowOffset);
                this.lowerRightShadow.setAttributeNS(null,"y",this.y + this.shadowOffset);
                this.lowerRightShadow.setAttributeNS(null,"width",this.width);
                this.lowerRightShadow.setAttributeNS(null,"height",this.height);
                this.lowerRightShadow.setAttributeNS(null,"points",this.x+","+(this.y+this.height)+" "+this.x+","+this.y+" "+(this.x+this.width)+","+this.y);
            }
            else if (this.buttonType == "ellipse") {
                this.lowerRightShadow = document.createElementNS(svgNS,"ellipse");
                this.lowerRightShadow.setAttributeNS(null,"cx",this.x + this.width * 0.5 + this.shadowOffset);
                this.lowerRightShadow.setAttributeNS(null,"cy",this.y + this.height * 0.5 + this.shadowOffset);
                this.lowerRightShadow.setAttributeNS(null,"rx",this.width * 0.5);
                this.lowerRightShadow.setAttributeNS(null,"ry",this.height * 0.5);
            }
            for (var attrib in this.shadeDarkStyles) {
                this.lowerRightShadow.setAttributeNS(null,attrib,this.shadeDarkStyles[attrib]);
            }
            this.parentGroup.appendChild(this.lowerRightShadow);
            
            //create buttonRect
            if (this.buttonType == "rect") {
                this.buttonRect = document.createElementNS(svgNS,"rect");
                this.buttonRect.setAttributeNS(null,"x",this.x);
                this.buttonRect.setAttributeNS(null,"y",this.y);
                this.buttonRect.setAttributeNS(null,"width",this.width);
                this.buttonRect.setAttributeNS(null,"height",this.height);
            }
            else if (this.buttonType == "ellipse") {
                this.buttonRect = document.createElementNS(svgNS,"ellipse");
                this.buttonRect.setAttributeNS(null,"cx",this.x + this.width * 0.5);
                this.buttonRect.setAttributeNS(null,"cy",this.y + this.height * 0.5);
                this.buttonRect.setAttributeNS(null,"rx",this.width * 0.5);
                this.buttonRect.setAttributeNS(null,"ry",this.height * 0.5);
            }
            for (var attrib in this.buttonStyles) {
                this.buttonRect.setAttributeNS(null,attrib,this.buttonStyles[attrib]);
            }    
            this.buttonRect.setAttributeNS(null,"cursor","pointer");
            this.buttonRect.addEventListener("mousedown",this,false);
            this.buttonRect.addEventListener("mouseup",this,false);
            this.buttonRect.addEventListener("click",this,false);
            this.parentGroup.appendChild(this.buttonRect);
            
            //call a helper method to create the text elements
            this.createButtonTexts();
                        
            if (this.buttonSymbolId != undefined) {
                this.buttonSymbolInstance = document.createElementNS(svgNS,"use");
                this.buttonSymbolInstance.setAttributeNS(null,"x",(this.x + this.width / 2));
                this.buttonSymbolInstance.setAttributeNS(null,"y",(this.y + this.height / 2));
                this.buttonSymbolInstance.setAttributeNS(xlinkNS,"href","#"+this.buttonSymbolId);
                this.buttonSymbolInstance.setAttributeNS(null,"pointer-events","none");
                this.parentGroup.appendChild(this.buttonSymbolInstance);
            }
            
            //create rectangle to deactivate the button
            if (this.buttonType == "rect") {
                this.deActivateRect = document.createElementNS(svgNS,"rect");
                this.deActivateRect.setAttributeNS(null,"x",this.x - this.shadowOffset);
                this.deActivateRect.setAttributeNS(null,"y",this.y - this.shadowOffset);
                this.deActivateRect.setAttributeNS(null,"width",this.width + this.shadowOffset * 2);
                this.deActivateRect.setAttributeNS(null,"height",this.height + this.shadowOffset * 2);
            }
            else if (this.buttonType == "ellipse") {
                this.deActivateRect = document.createElementNS(svgNS,"ellipse");
                this.deActivateRect.setAttributeNS(null,"cx",this.x + this.width * 0.5);
                this.deActivateRect.setAttributeNS(null,"cy",this.y + this.height * 0.5);
                this.deActivateRect.setAttributeNS(null,"rx",this.width * 0.5 + this.shadowOffset);
                this.deActivateRect.setAttributeNS(null,"ry",this.height * 0.5 + this.shadowOffset);
            }
        
            this.deActivateRect.setAttributeNS(null,"fill","white");
            this.deActivateRect.setAttributeNS(null,"fill-opacity","0.5");
            this.deActivateRect.setAttributeNS(null,"stroke","none");
            this.deActivateRect.setAttributeNS(null,"display","none");
            this.deActivateRect.setAttributeNS(null,"cursor","default");
            if (this.buttonStyles["rx"]) {
                this.deActivateRect.setAttributeNS(null,"rx",this.buttonStyles["rx"]);
            }
            if (this.buttonStyles["ry"]) {
                this.deActivateRect.setAttributeNS(null,"ry",this.buttonStyles["ry"]);
            }
            this.parentGroup.appendChild(this.deActivateRect);
        }
        else {
            alert("could not create or reference 'parentNode' of button with id '"+this.id+"'");            
        }
}

//test if parent group exists
button.prototype.testParent = function() {
    //test if of type object
    var nodeValid = false;
    this.parentGroup = document.createElementNS(svgNS,"g");
    if (typeof(this.parentNode) == "object") {
        if (this.parentNode.nodeName == "svg" || this.parentNode.nodeName == "g" || this.parentNode.nodeName == "svg:svg" || this.parentNode.nodeName == "svg:g") {
            this.parentNode.appendChild(this.parentGroup);
            nodeValid = true;
        }
    }
    else if (typeof(this.parentNode) == "string") { 
        //first test if button group exists
        if (!document.getElementById(this.parentNode)) {
            this.parentGroup.setAttributeNS(null,"id",this.parentNode);
            document.documentElement.appendChild(this.parentGroup);
            nodeValid = true;
           }
           else {
               document.getElementById(this.parentNode).appendChild(this.parentGroup);
               nodeValid = true;
           }
       }
       return nodeValid;
}

//move button to a new position
button.prototype.moveTo = function(moveX,moveY) {
    this.x = moveX;
    this.y = moveY;
    //case rect
    if (this.buttonType == "rect") {
        this.upperLeftShadow.setAttributeNS(null,"x",this.x - this.shadowOffset);
        this.upperLeftShadow.setAttributeNS(null,"y",this.y - this.shadowOffset);
        this.lowerRightShadow.setAttributeNS(null,"x",this.x + this.shadowOffset);
        this.lowerRightShadow.setAttributeNS(null,"y",this.y + this.shadowOffset);
        this.buttonRect.setAttributeNS(null,"x",this.x);
        this.buttonRect.setAttributeNS(null,"y",this.y);
    }
    else if (this.buttonType == "ellipse") {
        this.upperLeftShadow.setAttributeNS(null,"cx",this.x + this.width * 0.5 - this.shadowOffset);
        this.upperLeftShadow.setAttributeNS(null,"cy",this.y + this.height * 0.5 - this.shadowOffset);
        this.lowerRightShadow.setAttributeNS(null,"cx",this.x + this.width * 0.5 + this.shadowOffset);
        this.lowerRightShadow.setAttributeNS(null,"cy",this.y + this.height * 0.5 + this.shadowOffset);
        this.buttonRect.setAttributeNS(null,"cx",this.x + this.width * 0.5);
        this.buttonRect.setAttributeNS(null,"cy",this.y + this.height * 0.5);
    }
    //reposition button text
    if (this.buttonText != undefined) {
        this.parentGroup.removeChild(this.buttonTextElement);
        this.createButtonTexts();
    }
}

//resize button to a new width and height
button.prototype.resize = function(newWidth,newHeight) {
    this.width = newWidth;
    this.height = newHeight;
    //case rect
    if (this.buttonType == "rect") {
        this.upperLeftShadow.setAttributeNS(null,"width",this.width);
        this.upperLeftShadow.setAttributeNS(null,"height",this.height);
        this.lowerRightShadow.setAttributeNS(null,"width",this.width);
        this.lowerRightShadow.setAttributeNS(null,"height",this.height);
        this.buttonRect.setAttributeNS(null,"width",this.width);
        this.buttonRect.setAttributeNS(null,"height",this.height);
    }
    else if (this.buttonType == "ellipse") {
        this.upperLeftShadow.setAttributeNS(null,"rx",this.width * 0.5);
        this.upperLeftShadow.setAttributeNS(null,"ry",this.height * 0.5);
        this.lowerRightShadow.setAttributeNS(null,"rx",this.width * 0.5);
        this.lowerRightShadow.setAttributeNS(null,"ry",this.height * 0.5);
        this.buttonRect.setAttributeNS(null,"rx",this.width * 0.5);
        this.buttonRect.setAttributeNS(null,"ry",this.height * 0.5);
    }
    if (this.buttonText != undefined) {
        this.parentGroup.removeChild(this.buttonTextElement);
        this.createButtonTexts();
    }
}

//remove all button elements
button.prototype.removeButton = function() {
    this.parentGroup.removeChild(this.upperLeftShadow);
    this.parentGroup.removeChild(this.lowerRightShadow);
    this.parentGroup.removeChild(this.buttonRect);
    if (this.buttonTextElement) {
        this.parentGroup.removeChild(this.buttonTextElement);
    }
    if (this.buttonSymbolInstance) {
        this.parentGroup.removeChild(this.buttonSymbolInstance);
    }
    this.parentGroup.removeChild(this.deActivateRect);
}

//hide button
button.prototype.hideButton = function() {
    this.parentGroup.setAttributeNS(null,"display","none");
}

//show button
button.prototype.showButton = function() {
    this.parentGroup.setAttributeNS(null,"display","inherit");
}

//event handling
button.prototype.handleEvent = function(evt) {
    if (evt.type == "mousedown") {
        this.togglePressed("pressed");
        document.documentElement.addEventListener("mouseup",this,false);
    }
    if (evt.type == "mouseup") {
        this.togglePressed("released");
        document.documentElement.removeEventListener("mouseup",this,false);
    }
    if (evt.type == "click") {
        //for some strange reasons I could not forward the evt object here ;-(, the code below using a literal is a workaround
        //attention: only some of the evt properties are forwarded here, you can add more, if you need them
        var timerEvt = {x:evt.clientX,y:evt.clientY,type:evt.type,detail:evt.detail,timeStamp:evt.timeStamp}
        this.timer.setTimeout("fireFunction",this.timerMs,timerEvt);
    }
}

button.prototype.togglePressed = function(type) {
    if (type == "pressed") {
        for (var attrib in this.shadeDarkStyles) {
            this.upperLeftShadow.setAttributeNS(null,attrib,this.shadeDarkStyles[attrib]);
        }
        for (var attrib in this.shadeLightStyles) {
            this.lowerRightShadow.setAttributeNS(null,attrib,this.shadeLightStyles[attrib]);
        }
    }
    if (type == "released") {
        for (var attrib in this.shadeDarkStyles) {
            this.lowerRightShadow.setAttributeNS(null,attrib,this.shadeDarkStyles[attrib]);
        }
        for (var attrib in this.shadeLightStyles) {
            this.upperLeftShadow.setAttributeNS(null,attrib,this.shadeLightStyles[attrib]);
        }
    }
}

button.prototype.fireFunction = function(evt) {
    if (typeof(this.functionToCall) == "function") {
        if (this.buttonTextElement) {
            this.functionToCall(this.id,evt,this.buttonText);
        }
        if (this.buttonSymbolInstance) {
            this.functionToCall(this.id,evt);
        }
    }
    if (typeof(this.functionToCall) == "object") {
        if (this.buttonTextElement) {
            this.functionToCall.buttonPressed(this.id,evt,this.buttonText);
        }
        if (this.buttonSymbolInstance) {
            this.functionToCall.buttonPressed(this.id,evt);
        }
    }
    if (typeof(this.functionToCall) == undefined) {
        return;
    }
}

button.prototype.getTextValue = function() {
    return this.buttonText;
}

button.prototype.setTextValue = function(value) {
    this.buttonText = value;
    //remove previous buttonTextElement
    if (this.buttonTextElement) {
        this.parentGroup.removeChild(this.buttonTextElement);        
    }
    this.createButtonTexts();
}

//this is a helper function to create the text elements, it is used from two different methods, .createButton() and .setTextValue()
button.prototype.createButtonTexts = function() {
    if (this.buttonText != undefined) {
        //first see if there is a multiline button
        var buttonTexts = String(this.buttonText).split("\n");
        
        //set a start y-offset
        var dy = this.textStyles["font-size"]*1.1;
        
        //if you don't like our method for y-positioning here, you can experiment with the formula here and define your own "initY" 
        var initY = (this.height - dy * buttonTexts.length) / 2 + this.textStyles["font-size"];
        
        //create text element
        this.buttonTextElement = document.createElementNS(svgNS,"text");
        this.buttonTextElement.setAttributeNS(null,"x",(this.x + this.width / 2));
        this.buttonTextElement.setAttributeNS(null,"y",(this.y + initY));            
        for (var attrib in this.textStyles) {
            value = this.textStyles[attrib];
            this.buttonTextElement.setAttributeNS(null,attrib,value);
        }
        this.buttonTextElement.setAttributeNS(null,"pointer-events","none");
        this.buttonTextElement.setAttributeNS(null,"text-anchor","middle");
        this.buttonTextElement.setAttributeNS("http://www.w3.org/XML/1998/namespace","space","preserve");
        for (var j=0;j<buttonTexts.length;j++) {
            var tspan = document.createElementNS(svgNS,"tspan");
            tspan.setAttributeNS(null,"x",(this.x + this.width / 2));
            if (j == 0) {
                tspan.setAttributeNS(null,"dy",0);
            }
            else {
                tspan.setAttributeNS(null,"dy",dy);
            }
            var textNode = document.createTextNode(buttonTexts[j]);
            tspan.appendChild(textNode);
            this.buttonTextElement.appendChild(tspan);
        }
        this.parentGroup.appendChild(this.buttonTextElement);
    }    
}

button.prototype.activate = function(value) {
    this.deActivateRect.setAttributeNS(null,"display","none");
    this.activated = true;
}

button.prototype.deactivate = function(value) {
    this.deActivateRect.setAttributeNS(null,"display","inherit");
    this.parentGroup.appendChild(this.deActivateRect);
    this.activated = false;
}

//switchbutton
//initialize inheritance
switchbutton.prototype = new button();
switchbutton.prototype.constructor = switchbutton;
switchbutton.superclass = button.prototype;

function switchbutton(id,parentNode,functionToCall,buttonType,buttonText,buttonSymbolId,x,y,width,height,textStyles,buttonStyles,shadeLightStyles,shadeDarkStyles,shadowOffset) {
    var nrArguments = 15;
    if (arguments.length > 0) {
        if (arguments.length == nrArguments) {
            this.init(id,parentNode,functionToCall,buttonType,buttonText,buttonSymbolId,x,y,width,height,textStyles,buttonStyles,shadeLightStyles,shadeDarkStyles,shadowOffset);
        }
        else {
            alert("Error in Switchbutton ("+id+"): wrong nr of arguments! You have to pass over "+nrArguments+" parameters.");        
        }        
    }
}

switchbutton.prototype.init = function(id,parentNode,functionToCall,buttonType,buttonText,buttonSymbolId,x,y,width,height,textStyles,buttonStyles,shadeLightStyles,shadeDarkStyles,shadowOffset) {
    switchbutton.superclass.init.call(this,id,parentNode,functionToCall,buttonType,buttonText,buttonSymbolId,x,y,width,height,textStyles,buttonStyles,shadeLightStyles,shadeDarkStyles,shadowOffset);
    this.on = false;
}

//overwriting handleEventcode
switchbutton.prototype.handleEvent = function(evt) {
    //for some strange reasons I could not forward the evt object here ;-(, the code below using a literal is a workaround
    //attention: only some of the evt properties are forwarded here, you can add more, if you need them
    var timerEvt = {x:evt.clientX,y:evt.clientY,type:evt.type,detail:evt.detail,timeStamp:evt.timeStamp}
    if (evt.type == "click") {
        if (this.on) {
            this.on = false;
            this.togglePressed("released");
            this.timer.setTimeout("fireFunction",this.timerMs,timerEvt);
        }
        else {
            this.on = true;
            this.togglePressed("pressed");
            this.timer.setTimeout("fireFunction",this.timerMs,timerEvt);
        }
    }
}

switchbutton.prototype.getSwitchValue = function() {
    return this.on;
}

switchbutton.prototype.setSwitchValue = function(onOrOff,firefunction) {
    this.on = onOrOff;
    //artificial timer event - don't use the values!
    var timerEvt = {x:0,y:0,type:"click",detail:1,timeStamp:0}
    if (this.on) {
        this.togglePressed("pressed");
        if (firefunction) {
            this.timer.setTimeout("fireFunction",this.timerMs,timerEvt);
        }
    }
    else {
        this.togglePressed("released");
        if (firefunction) {
            this.timer.setTimeout("fireFunction",this.timerMs,timerEvt);
        }
    }
}

//overwriting fireFunction code
switchbutton.prototype.fireFunction = function(evt) {
    if (typeof(this.functionToCall) == "function") {
        if (this.buttonTextElement) {
            this.functionToCall(this.id,evt,this.on,this.buttonText);
        }
        if (this.buttonSymbolInstance) {
            this.functionToCall(this.id,evt,this.on);
        }
    }
    if (typeof(this.functionToCall) == "object") {
        if (this.buttonTextElement) {
            this.functionToCall.buttonPressed(this.id,evt,this.on,this.buttonText);
        }
        if (this.buttonSymbolInstance) {
            this.functionToCall.buttonPressed(this.id,evt,this.on);
        }
    }
    if (typeof(this.functionToCall) == undefined) {
        return;
    }
}
