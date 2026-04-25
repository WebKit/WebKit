/*
Scripts to create interactive colour pickers in SVG using ECMA script
Copyright (C) <2006>  <Andreas Neumann>
Version 1.0, 2006-10-26
neumann@karto.baug.ethz.ch
http://www.carto.net/
http://www.carto.net/neumann/

Credits:
* none so far

----

Documentation: http://www.carto.net/papers/svg/gui/colourPicker/

----

current version: 1.0

version history:
1.0 (2006-10-26)
initial version

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

original document site: http://www.carto.net/papers/svg/gui/colourPicker/
Please contact the author in case you want to use code or ideas commercially.
If you use this code, please include this copyright header, the included full
LGPL 2.1 text and read the terms provided in the LGPL 2.1 license
(http://www.gnu.org/copyleft/lesser.txt)

-------------------------------

Please report bugs and send improvements to neumann@karto.baug.ethz.ch
If you use this control, please link to the original (http://www.carto.net/papers/svg/gui/colourPicker/)
somewhere in the source-code-comment or the "about" of your project and give credits, thanks!

*/

function colourPicker(id,parentNode,x,y,width,height,bgstyles,textstyles,sliderSymbId,satSliderVisible,valSliderVisible,alphaSliderVisible,colValTextVisible,fillVisible,strokeVisible,startHue,endHue,nrStopVals,fillStartColor,strokeStartColor,functionToCall) {
    var nrArguments = 21;
    var createColourPicker= true;
    if (arguments.length == nrArguments) {    
        this.id = id; //the id of the colour picker
        this.parentNode = parentNode; //the id or node reference of the parent group where the button can be appended
        this.x = x; //upper left corner of colour picker
        this.y = y; //upper left corner of colour picker
        this.width = width; //width of colour picker
        this.height = height; //height of colour picker
        this.bgstyles = bgstyles; //an array of literals containing presentation attributes of the colourpicker background
        this.textstyles = textstyles; //if shadowcolor = none than no shadow will be used
        this.sliderSymbId = sliderSymbId; //id referencing the slider symbol to be used for the sliders
        this.satSliderVisible = satSliderVisible; //possible values for visibility: true/false
        this.valSliderVisible = valSliderVisible; //possible values for visibility: true/false
        this.alphaSliderVisible = alphaSliderVisible; //possible values for visibility: true/false
        this.colValTextVisible = colValTextVisible; //possible values for visibility: true/false
        this.fillVisible = fillVisible; //possible values for visibility: true/false
        this.strokeVisible = strokeVisible; //possible values for visibility: true/false
        this.startHue = startHue; //start hue in degree (0 to 360)
        this.endHue = endHue; //end hue in degree (0 to 360), end hue should be larger than start hue
        this.nrStopVals = nrStopVals; //nr of stop vals in between, in addition to start and end
        this.fillStartColors = fillStartColor.split(","); //string, rgba.format, f.e. 255,0,0,1
        this.strokeStartColors = strokeStartColor.split(","); //string, rgba.format, f.e. 255,0,0,1
        this.functionToCall = functionToCall; //a function or object to call, in case of an object, the method "getColor" is called; if you don't need any, than just say undefined
        if (this.fillVisible) {
            this.activeColourType = "fill";
        }
        else {
            this.activeColourType = "stroke";            
        }
        //now test if defs section is available
        try {
            this.svgdefs = document.getElementsByTagNameNS(svgNS,"defs").item(0);
        }
        catch (ex) {
            createColourPicker = false;
            alert("Error "+ex+", Could not find a <defs /> section in your svg-file!\nPlease add at least an empty <defs /> element!");
        }
        this.created = false;        
    }
    else {
        createColourPicker = false;
        alert("Error in colourPicker ("+id+") constructor: wrong nr of arguments! You have to pass over "+nrArguments+" parameters.");
    }
    if (createColourPicker) {
        this.createColourpicker();
    }
    else {
        alert("Could not create colourPicker with id '"+id+"' due to errors in the constructor parameters");        
    }
}

//test if parent group exists
colourPicker.prototype.testParent = function() {
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


colourPicker.prototype.createColourpicker = function() {    
    var result = this.testParent();
    if (result) {
        this.nrSliders = 1; //these are to define the distribution along y-axis
        this.nrRects = 0; //these are to define the distribution along x-axis

        //create gradients in defs section
        //hueGradient
        var hueGradientDefs = document.createElementNS(svgNS,"linearGradient");
        hueGradientDefs.setAttributeNS(null,"id",this.id+"_hueGradient");
        var curHue = this.startHue;
        var curPerc = 0;
        var hueIncr = (this.endHue - this.startHue) / (this.nrStopVals - 1);
        var percIncr = 100 / (this.nrStopVals - 1);
        for (i = 0;i<this.nrStopVals;i++) {
            if (curPerc > 100) {
                curPerc = 100;
            }
            if (curHue > this.endHue) {
                curHue = this.endHue;
            }
            var stopVal = document.createElementNS(svgNS,"stop");
            stopVal.setAttributeNS(null,"offset",curPerc+"%");
            myRgbArr = hsv2rgb(Math.round(curHue),1,1);
            stopVal.setAttributeNS(null,"stop-color","rgb("+myRgbArr['red']+","+myRgbArr['green']+","+myRgbArr['blue']+")");
            hueGradientDefs.appendChild(stopVal);
            curPerc += percIncr;
            curHue += hueIncr;
        }
        if (this.created) {
            this.svgdefs.replaceChild(hueGradientDefs,document.getElementById(this.id+"_hueGradient"));
        }
        else {
            this.svgdefs.appendChild(hueGradientDefs);                        
        }
        //generate satGradient if visible
        if (this.satSliderVisible) {
            var satGradientDefs = document.createElementNS(svgNS,"linearGradient");
            satGradientDefs.setAttributeNS(null,"id",this.id+"_satGradient");
            this.satStartGradient = document.createElementNS(svgNS,"stop");
            this.satStartGradient.setAttributeNS(null,"offset","0%");
            this.satStartGradient.setAttributeNS(null,"stop-color","rgb(255,255,255)");
            satGradientDefs.appendChild(this.satStartGradient);
            this.satEndGradient = document.createElementNS(svgNS,"stop");
            this.satEndGradient.setAttributeNS(null,"offset","100%");
            this.satEndGradient.setAttributeNS(null,"stop-color","rgb(255,0,0)");
            satGradientDefs.appendChild(this.satEndGradient);
            if (this.created) {
                this.svgdefs.replaceChild(satGradientDefs,document.getElementById(this.id+"_satGradient"));
            }
            else {
                this.svgdefs.appendChild(satGradientDefs);                        
            }
            this.nrSliders++;
        }
        //generate valGradient if visible
        if (this.valSliderVisible) {
            var valGradientDefs = document.createElementNS(svgNS,"linearGradient");
            valGradientDefs.setAttributeNS(null,"id",this.id+"_valGradient");
            this.valStartGradient = document.createElementNS(svgNS,"stop");
            this.valStartGradient.setAttributeNS(null,"offset","0%");
            this.valStartGradient.setAttributeNS(null,"stop-color","rgb(0,0,0)");
            valGradientDefs.appendChild(this.valStartGradient);
            this.valEndGradient = document.createElementNS(svgNS,"stop");
            this.valEndGradient.setAttributeNS(null,"offset","100%");
            this.valEndGradient.setAttributeNS(null,"stop-color","rgb(255,0,0)");
            valGradientDefs.appendChild(this.valEndGradient);
            if (this.created) {
                this.svgdefs.replaceChild(valGradientDefs,document.getElementById(this.id+"_valGradient"));
            }
            else {
                this.svgdefs.appendChild(valGradientDefs);                        
            }
            this.nrSliders++;
        }
        //generate opacityGradient if visible
        if (this.alphaSliderVisible) {
            var alphaGradientDefs = document.createElementNS(svgNS,"linearGradient");
            alphaGradientDefs.setAttributeNS(null,"id",this.id+"_alphaGradient");
            this.alphaStartGradient = document.createElementNS(svgNS,"stop");
            this.alphaStartGradient.setAttributeNS(null,"offset","0%");
            this.alphaStartGradient.setAttributeNS(null,"stop-color","rgb(255,0,0)");
            this.alphaStartGradient.setAttributeNS(null,"stop-opacity",0);
            alphaGradientDefs.appendChild(this.alphaStartGradient );
            this.alphaEndGradient  = document.createElementNS(svgNS,"stop");
            this.alphaEndGradient.setAttributeNS(null,"offset","100%");
            this.alphaEndGradient.setAttributeNS(null,"stop-color","rgb(255,0,0)");
            this.alphaEndGradient.setAttributeNS(null,"stop-opacity",1);
            alphaGradientDefs.appendChild(this.alphaEndGradient);
            if (this.created) {
                this.svgdefs.replaceChild(alphaGradientDefs,document.getElementById(this.id+"_alphaGradient"));
            }
            else {
                this.svgdefs.appendChild(alphaGradientDefs);                        
            }
            this.nrSliders++;
        }
    
        //create bg rectangle
        var bgRect = document.createElementNS(svgNS,"rect");
        bgRect.setAttributeNS(null,"x",this.x);
        bgRect.setAttributeNS(null,"y",this.y);
        bgRect.setAttributeNS(null,"width",this.width);
        bgRect.setAttributeNS(null,"height",this.height);
        for (var attrib in this.bgstyles) {
            bgRect.setAttributeNS(null,attrib,this.bgstyles[attrib]);
        }    
        this.parentGroup.appendChild(bgRect);
        
        //now createSliders
        var invisSliderLineWidth = this.width * 0.05;
        var slidRectHeight = invisSliderLineWidth * 0.5;
        var sliderStyles = {"stroke":"none","fill":"none"};
        var yNew = this.y + this.height * 0.13;
        var yNewInc = this.height * 0.8 / this.nrSliders;
        //create hue slider
        //first create rectangle behind slider to hold gradient
        this.rectHue = document.createElementNS(svgNS,"rect");
        this.rectHue.setAttributeNS(null,"x",(this.x + this.width * 0.04));
        this.rectHue.setAttributeNS(null,"y",(yNew - slidRectHeight * 0.5));
        this.rectHue.setAttributeNS(null,"width",(this.x + this.width * 0.7)-(this.x + this.width * 0.04));
        this.rectHue.setAttributeNS(null,"height",slidRectHeight);
        this.rectHue.setAttributeNS(null,"fill","url(#"+this.id+"_hueGradient)");
        this.parentGroup.appendChild(this.rectHue);
        this.slidHue = new slider("hueSlider_"+this.id,this.parentGroup,this.x + this.width * 0.04,yNew,this.startHue,this.x + this.width * 0.7,yNew,this.endHue,0,sliderStyles,invisSliderLineWidth,this.sliderSymbId,this,true);
        var hueText = document.createElementNS(svgNS,"text");
        hueText.setAttributeNS(null,"x",this.slidHue.x2 + this.width * 0.02);
        hueText.setAttributeNS(null,"y",this.slidHue.y2 + this.width * 0.015);
        hueText.setAttributeNS(null,"pointer-events","none");
        for (var attrib in this.textstyles) {
            hueText.setAttributeNS(null,attrib,this.textstyles[attrib]);
        }    
        var hueTextNode = document.createTextNode("Hue ("+this.startHue+String.fromCharCode(176)+"-"+this.endHue+String.fromCharCode(176)+")");
        hueText.appendChild(hueTextNode);
        this.parentGroup.appendChild(hueText);

        //create rects to show colour values for fill and stroke
        if (this.strokeVisible) {
            this.strokeRect = document.createElementNS(svgNS,"rect");
            if (this.fillVisible) {
                this.strokeRect.setAttributeNS(null,"x",this.slidHue.x1 + this.width * 0.1);
                this.strokeRect.setAttributeNS(null,"y",this.y + this.height * 0.45);
            }
            else {
                this.strokeRect.setAttributeNS(null,"x",this.slidHue.x1);
                this.strokeRect.setAttributeNS(null,"y",this.y + this.height * 0.3);        
            }
            this.strokeRect.setAttributeNS(null,"width",this.width * 0.15);
            this.strokeRect.setAttributeNS(null,"height",this.height * 0.3);
            this.strokeRect.setAttributeNS(null,"fill","none");
            this.strokeRect.setAttributeNS(null,"stroke","rgb("+this.strokeStartColors[0]+","+this.strokeStartColors[1]+","+this.strokeStartColors[2]+")");
            this.strokeRect.setAttributeNS(null,"stroke-width",invisSliderLineWidth * 0.5);
            this.strokeRect.setAttributeNS(null,"stroke-opacity",this.strokeStartColors[3]);
            this.strokeRect.setAttributeNS(null,"id",this.id+"_strokeColour");
            this.strokeRect.setAttributeNS(null,"pointer-events","all");
            this.strokeRect.addEventListener("click",this,false);
            this.parentGroup.appendChild(this.strokeRect);
            this.nrRects++;
        }
        if (this.fillVisible) {
            this.fillRect = document.createElementNS(svgNS,"rect");
            this.fillRect.setAttributeNS(null,"x",this.slidHue.x1);
            this.fillRect.setAttributeNS(null,"y",this.y + this.height * 0.3);
            this.fillRect.setAttributeNS(null,"width",this.width * 0.15);
            this.fillRect.setAttributeNS(null,"height",this.height * 0.3);
            this.fillRect.setAttributeNS(null,"fill","rgb("+this.fillStartColors[0]+","+this.fillStartColors[1]+","+this.fillStartColors[2]+")");
            this.fillRect.setAttributeNS(null,"fill-opacity",this.fillStartColors[3]);
            this.fillRect.setAttributeNS(null,"id",this.id+"_fillColour");
            this.fillRect.addEventListener("click",this,false);
            if (this.activeColourType == "stroke") {
                this.parentGroup.insertBefore(this.fillRect,this.strokeRect);            
            }
            else {
                this.parentGroup.appendChild(this.fillRect);
            }
            this.nrRects++;
        }
    
        switch(this.nrRects) {
            case 0:
                slidLeft = this.slidHue.x1;
                break;
            case 1:
                slidLeft = this.slidHue.x1 + this.width * 0.2;
                break;
            default:
                slidLeft = this.slidHue.x1 + this.width * 0.3;        
                break;
        }
    
        yNew = yNew + yNewInc;
        if (this.satSliderVisible) {
            //create rectangle to hold sat gradient
            this.rectSat = document.createElementNS(svgNS,"rect");
            this.rectSat.setAttributeNS(null,"x",slidLeft);
            this.rectSat.setAttributeNS(null,"y",(yNew - slidRectHeight * 0.5));
            this.rectSat.setAttributeNS(null,"width",(this.x + this.width * 0.7)-slidLeft);
            this.rectSat.setAttributeNS(null,"height",slidRectHeight);
            this.rectSat.setAttributeNS(null,"fill","url(#"+this.id+"_satGradient)");
            this.parentGroup.appendChild(this.rectSat);
            //create sat slider
            this.slidSat = new slider("satSlider_"+this.id,this.parentGroup,slidLeft,yNew,0,this.x + this.width * 0.7,yNew,100,100,sliderStyles,invisSliderLineWidth,this.sliderSymbId,this,true);
            var satText = document.createElementNS(svgNS,"text");
            for (var attrib in this.textstyles) {
                satText.setAttributeNS(null,attrib,this.textstyles[attrib]);
            }    
            satText.setAttributeNS(null,"pointer-events","none");
            satText.setAttributeNS(null,"x",this.slidSat.x2 + this.width * 0.02);
            satText.setAttributeNS(null,"y",this.slidSat.y2 + this.width * 0.015);
            var satTextNode = document.createTextNode("Saturation (%)");
            satText.appendChild(satTextNode);
            this.parentGroup.appendChild(satText);
            yNew = yNew + yNewInc;
        }
        if (this.valSliderVisible) {
            //create rectangle to hold val gradient
            this.rectVal = document.createElementNS(svgNS,"rect");
            this.rectVal.setAttributeNS(null,"x",slidLeft);
            this.rectVal.setAttributeNS(null,"y",(yNew - slidRectHeight * 0.5));
            this.rectVal.setAttributeNS(null,"width",(this.x + this.width * 0.7)-slidLeft);
            this.rectVal.setAttributeNS(null,"height",slidRectHeight);
            this.rectVal.setAttributeNS(null,"fill","url(#"+this.id+"_valGradient)");
            this.parentGroup.appendChild(this.rectVal);
            //create val slider
            this.slidVal = new slider("valSlider_"+this.id,this.parentGroup,slidLeft,yNew,0,this.x + this.width * 0.7,yNew,100,100,sliderStyles,invisSliderLineWidth,this.sliderSymbId,this,true);
            var valText = document.createElementNS(svgNS,"text");
            for (var attrib in this.textstyles) {
                valText.setAttributeNS(null,attrib,this.textstyles[attrib]);
            }    
            valText.setAttributeNS(null,"pointer-events","none");
            valText.setAttributeNS(null,"x",this.slidVal.x2 + this.width * 0.02);
            valText.setAttributeNS(null,"y",this.slidVal.y2 + this.width * 0.015);
            var valTextNode = document.createTextNode("Value (%)");
            valText.appendChild(valTextNode);
            this.parentGroup.appendChild(valText);
            yNew = yNew + yNewInc;
        }
        if (this.alphaSliderVisible) {
            //create rectangle to hold alpha gradient
            this.rectAlpha = document.createElementNS(svgNS,"rect");
            this.rectAlpha.setAttributeNS(null,"x",slidLeft);
            this.rectAlpha.setAttributeNS(null,"y",(yNew - slidRectHeight * 0.5));
            this.rectAlpha.setAttributeNS(null,"width",(this.x + this.width * 0.7)-slidLeft);
            this.rectAlpha.setAttributeNS(null,"height",slidRectHeight);
            this.rectAlpha.setAttributeNS(null,"fill","url(#"+this.id+"_alphaGradient)");
            this.parentGroup.appendChild(this.rectAlpha);
            //create val slider
            this.slidAlpha = new slider("alphaSlider_"+this.id,this.parentGroup,slidLeft,yNew,0,this.x + this.width * 0.7,yNew,100,100,sliderStyles,invisSliderLineWidth,this.sliderSymbId,this,true);
            var alphaText = document.createElementNS(svgNS,"text");
            for (var attrib in this.textstyles) {
                alphaText.setAttributeNS(null,attrib,this.textstyles[attrib]);
            }    
            alphaText.setAttributeNS(null,"pointer-events","none");
            alphaText.setAttributeNS(null,"x",this.slidAlpha.x2 + this.width * 0.02);
            alphaText.setAttributeNS(null,"y",this.slidAlpha.y2 + this.width * 0.015);
            var alphaTextNode = document.createTextNode("Opacity (%)");
            alphaText.appendChild(alphaTextNode);
            this.parentGroup.appendChild(alphaText);
        }
        //create text to hold rgb/hsv values
        if (this.colValTextVisible) {
            this.colValText = document.createElementNS(svgNS,"text");
            for (var attrib in this.textstyles) {
                this.colValText.setAttributeNS(null,attrib,this.textstyles[attrib]);
            }    
            this.colValText.setAttributeNS(null,"x",this.slidHue.x1);
            this.colValText.setAttributeNS(null,"y",this.y + this.height * 0.95);
            var textNode = document.createTextNode("RGB: 255,0,0; HSV: 0,100,100");
            this.colValText.appendChild(textNode);
            this.parentGroup.appendChild(this.colValText);
        }
            
        fillHSVvar = rgb2hsv(parseInt(this.fillStartColors[0]),parseInt(this.fillStartColors[1]),parseInt(this.fillStartColors[2]));
        strokeHSVvar = rgb2hsv(parseInt(this.strokeStartColors[0]),parseInt(this.strokeStartColors[1]),parseInt(this.strokeStartColors[2]));
        this.hue = new Array();
        this.hue["fill"] = fillHSVvar["hue"];
        this.hue["stroke"] = strokeHSVvar["hue"];
        this.sat = new Array();
        this.sat["fill"] = fillHSVvar["sat"];
        this.sat["stroke"] = strokeHSVvar["sat"];
        this.val = new Array();
        this.val["fill"] = fillHSVvar["val"];
        this.val["stroke"] = strokeHSVvar["val"];
        this.alpha = new Array();
        this.alpha["fill"] = parseFloat(this.fillStartColors[3]);
        this.alpha["stroke"] = parseFloat(this.strokeStartColors[3]);
        this.rgbArr = new Array();
        this.rgbArr["fill"] = hsv2rgb(this.hue["fill"],this.sat["fill"],this.val["fill"]);
        this.rgbArr["stroke"] = hsv2rgb(this.hue["stroke"],this.sat["stroke"],this.val["stroke"]);
        this.setRGBAColour(this.activeColourType,this.rgbArr[this.activeColourType]["red"],this.rgbArr[this.activeColourType]["green"],this.rgbArr[this.activeColourType]["blue"],this.alpha[this.activeColourType],false);
        
        //indicate that it was created once
        this.created = true;
    }
    else {
        alert("could not create or reference 'parentNode' of button with id '"+this.id+"'");            
    }
}

colourPicker.prototype.handleEvent = function(evt) {
    var el = evt.target;
    if (evt.type == "click") {
        var id = el.getAttributeNS(null,"id");
        if (id == this.id+"_fillColour") {
            this.activeColourType = "fill";
            if (this.strokeVisible) {
                this.parentGroup.insertBefore(this.strokeRect,this.fillRect);
            }
        }
        if (id == this.id+"_strokeColour") {
            this.activeColourType = "stroke";
            if (this.fillVisible) {
                this.parentGroup.insertBefore(this.fillRect,this.strokeRect);
            }
        }
        if (id == this.id+"_fillColour" || id == this.id+"_strokeColour") {
            this.setRGBAColour(this.activeColourType,this.rgbArr[this.activeColourType]["red"],this.rgbArr[this.activeColourType]["green"],this.rgbArr[this.activeColourType]["blue"],this.alpha[this.activeColourType],false);
        }
    }
}

//this method should be used from external
colourPicker.prototype.getValues = function() {
    var result = {"fill":{"red":this.rgbArr["fill"]["red"],"green":this.rgbArr["fill"]["green"],"blue":this.rgbArr["fill"]["blue"],"alpha":this.alpha["fill"],"hue":this.hue["fill"],"sat":this.sat["fill"],"val":this.val["fill"]},"stroke":{"red":this.rgbArr["stroke"]["red"],"green":this.rgbArr["stroke"]["green"],"blue":this.rgbArr["stroke"]["blue"],"alpha":this.alpha["stroke"],"hue":this.hue["stroke"],"sat":this.sat["stroke"],"val":this.val["stroke"]}};
    return result;
}

//this method is used by this object but may be also used from external
colourPicker.prototype.setRGBAColour = function(colourType,red,green,blue,alpha,fireFunction) {
    //colourType = fill|stroke
    hsvArr = rgb2hsv(red,green,blue);
    this.hue[colourType] = hsvArr["hue"];
    this.sat[colourType] = hsvArr["sat"];
    this.val[colourType] = hsvArr["val"];
    this.alpha[colourType] = alpha;
    this.rgbArr[colourType]["red"] = red;
    this.rgbArr[colourType]["green"] = green;
    this.rgbArr[colourType]["blue"] = blue;
    if (colourType == this.activeColourType) {
        this.slidHue.setValue(this.hue[colourType]);
        if (this.satSliderVisible) {
            this.slidSat.setValue(Math.round(this.sat[colourType] * 100));
            rgbSatEnd = hsv2rgb(this.hue[this.activeColourType],1,this.val[this.activeColourType]);
            rgbSatStart = hsv2rgb(this.hue[this.activeColourType],0,this.val[this.activeColourType]);
            this.satStartGradient.setAttributeNS(null,"stop-color","rgb("+rgbSatStart["red"]+","+rgbSatStart["green"]+","+rgbSatStart["blue"]+")");
            this.satEndGradient.setAttributeNS(null,"stop-color","rgb("+rgbSatEnd["red"]+","+rgbSatEnd["green"]+","+rgbSatEnd["blue"]+")");
            if (myMapApp.navigator == "Batik") {
                this.rectSat.removeAttributeNS(null,"fill");
                this.rectSat.setAttributeNS(null,"fill","url(#"+this.id+"_satGradient)");
            }
        }
        if (this.valSliderVisible) {
            this.slidVal.setValue(Math.round(this.val[colourType] * 100));
            rgbValEnd = hsv2rgb(this.hue[this.activeColourType],this.sat[this.activeColourType],1);
            rgbValStart = hsv2rgb(this.hue[this.activeColourType],this.sat[this.activeColourType],0);
            this.valStartGradient.setAttributeNS(null,"stop-color","rgb("+rgbValStart["red"]+","+rgbValStart["green"]+","+rgbValStart["blue"]+")");
            this.valEndGradient.setAttributeNS(null,"stop-color","rgb("+rgbValEnd["red"]+","+rgbValEnd["green"]+","+rgbValEnd["blue"]+")");
            if (myMapApp.navigator == "Batik") {
                this.rectVal.removeAttributeNS(null,"fill");
                this.rectVal.setAttributeNS(null,"fill","url(#"+this.id+"_valGradient)");
            }
        }
        if (this.alphaSliderVisible) {
            this.slidAlpha.setValue(Math.round(this.alpha[colourType] * 100));
            rgbAlpha = hsv2rgb(this.hue[this.activeColourType],this.sat[this.activeColourType],this.val[this.activeColourType]);
            this.alphaStartGradient.setAttributeNS(null,"stop-color","rgb("+rgbAlpha["red"]+","+rgbAlpha["green"]+","+rgbAlpha["blue"]+")");
            this.alphaEndGradient.setAttributeNS(null,"stop-color","rgb("+rgbAlpha["red"]+","+rgbAlpha["green"]+","+rgbAlpha["blue"]+")");
            if (myMapApp.navigator == "Batik") {
                this.rectAlpha.removeAttributeNS(null,"fill");
                this.rectAlpha.setAttributeNS(null,"fill","url(#"+this.id+"_alphaGradient)");
            }
        }
        if (this.colValTextVisible) {
            this.colValText.firstChild.nodeValue = "RGBA: "+this.rgbArr[this.activeColourType]["red"]+","+this.rgbArr[this.activeColourType]["green"]+","+this.rgbArr[this.activeColourType]["blue"]+","+Math.round(this.alpha[this.activeColourType] * 100) +"; HSVA: "+this.hue[this.activeColourType]+","+Math.round(this.sat[this.activeColourType] * 100)+","+Math.round(this.val[this.activeColourType] * 100)+","+Math.round(this.alpha[this.activeColourType] * 100);
        }
    }
    //set values in fill and stroke rectangles
    if (colourType == "stroke") {
        this.strokeRect.setAttributeNS(null,"stroke","rgb("+red+","+green+","+blue+")");
        this.strokeRect.setAttributeNS(null,"stroke-opacity",alpha);
    }
    if (colourType == "fill") {
        this.fillRect.setAttributeNS(null,"fill","rgb("+red+","+green+","+blue+")");
        this.fillRect.setAttributeNS(null,"fill-opacity",alpha);
    }
    if (fireFunction) {
        this.fireFunction();
    }
}

//this is a helper method to be used by this object only
colourPicker.prototype.getSliderVal = function(changeType,sliderId,slidValue) {
    //get all colour values from sliders
    if (sliderId == "hueSlider_"+this.id) {
        this.hue[this.activeColourType] = Math.round(this.slidHue.value);
    }
    if (sliderId == "satSlider_"+this.id && this.satSliderVisible) {
        this.sat[this.activeColourType] = Math.round(this.slidSat.value) / 100;
    }
    if (sliderId == "valSlider_"+this.id && this.valSliderVisible) {
        this.val[this.activeColourType] = Math.round(this.slidVal.value) / 100;
    }
    if (sliderId == "alphaSlider_"+this.id && this.alphaSliderVisible) {
        this.alpha[this.activeColourType] = Math.round(this.slidAlpha.value) / 100;
    }
    this.rgbArr[this.activeColourType] = hsv2rgb(this.hue[this.activeColourType],this.sat[this.activeColourType],this.val[this.activeColourType]);
    if (this.fillVisible && this.activeColourType == "fill") {
        this.fillRect.setAttributeNS(null,"fill","rgb("+this.rgbArr["fill"]["red"]+","+this.rgbArr["fill"]["green"]+","+this.rgbArr["fill"]["blue"]+")");
        this.fillRect.setAttributeNS(null,"fill-opacity",this.alpha[this.activeColourType]);
    }
    if (this.strokeVisible && this.activeColourType == "stroke") {
        this.strokeRect.setAttributeNS(null,"stroke","rgb("+this.rgbArr["stroke"]["red"]+","+this.rgbArr["stroke"]["green"]+","+this.rgbArr["stroke"]["blue"]+")");
        this.strokeRect.setAttributeNS(null,"stroke-opacity",this.alpha[this.activeColourType]);
    }
    if (this.colValTextVisible) {
        this.colValText.firstChild.nodeValue = "RGBA: "+this.rgbArr[this.activeColourType]["red"]+","+this.rgbArr[this.activeColourType]["green"]+","+this.rgbArr[this.activeColourType]["blue"]+","+Math.round(this.alpha[this.activeColourType] * 100) +"; HSVA: "+this.hue[this.activeColourType]+","+Math.round(this.sat[this.activeColourType] * 100)+","+Math.round(this.val[this.activeColourType] * 100)+","+Math.round(this.alpha[this.activeColourType] * 100);
    }
    if (this.satSliderVisible) {
        rgbSatEnd = hsv2rgb(this.hue[this.activeColourType],1,this.val[this.activeColourType]);
        rgbSatStart = hsv2rgb(this.hue[this.activeColourType],0,this.val[this.activeColourType]);
        this.satStartGradient.setAttributeNS(null,"stop-color","rgb("+rgbSatStart["red"]+","+rgbSatStart["green"]+","+rgbSatStart["blue"]+")");
        this.satEndGradient.setAttributeNS(null,"stop-color","rgb("+rgbSatEnd["red"]+","+rgbSatEnd["green"]+","+rgbSatEnd["blue"]+")");
        if (myMapApp.navigator == "Batik") {
            this.rectSat.removeAttributeNS(null,"fill");
            this.rectSat.setAttributeNS(null,"fill","url(#"+this.id+"_satGradient)");
        }
    }
    if (this.valSliderVisible) {
        rgbValEnd = hsv2rgb(this.hue[this.activeColourType],this.sat[this.activeColourType],1);
        rgbValStart = hsv2rgb(this.hue[this.activeColourType],this.sat[this.activeColourType],0);
        this.valStartGradient.setAttributeNS(null,"stop-color","rgb("+rgbValStart["red"]+","+rgbValStart["green"]+","+rgbValStart["blue"]+")");
        this.valEndGradient.setAttributeNS(null,"stop-color","rgb("+rgbValEnd["red"]+","+rgbValEnd["green"]+","+rgbValEnd["blue"]+")");
        if (myMapApp.navigator == "Batik") {
            this.rectVal.removeAttributeNS(null,"fill");
            this.rectVal.setAttributeNS(null,"fill","url(#"+this.id+"_valGradient)");
        }
    }
    if (this.alphaSliderVisible) {
        rgbAlpha = hsv2rgb(this.hue[this.activeColourType],this.sat[this.activeColourType],this.val[this.activeColourType]);
        this.alphaStartGradient.setAttributeNS(null,"stop-color","rgb("+rgbAlpha["red"]+","+rgbAlpha["green"]+","+rgbAlpha["blue"]+")");
        this.alphaEndGradient.setAttributeNS(null,"stop-color","rgb("+rgbAlpha["red"]+","+rgbAlpha["green"]+","+rgbAlpha["blue"]+")");
        if (myMapApp.navigator == "Batik") {
            this.rectAlpha.removeAttributeNS(null,"fill");
            this.rectAlpha.setAttributeNS(null,"fill","url(#"+this.id+"_alphaGradient)");
        }
    }
    this.fireFunction();
}

//this hides a colourPicker
colourPicker.prototype.hide = function() {
    this.parentGroup.setAttributeNS(null,"display","none");
}

//this method shows a colourPicker once it was hidden
colourPicker.prototype.show = function() {
    this.parentGroup.setAttributeNS(null,"display","inherit");
}

//this is an internal method
colourPicker.prototype.fireFunction = function() {
    var values = this.getValues();
    if (typeof(this.functionToCall) == "function") {
        this.functionToCall(this.id,values);
    }
    if (typeof(this.functionToCall) == "object") {
        this.functionToCall.colourChanged(this.id,values);
    }
    if (typeof(this.functionToCall) == undefined) {
        return;
    }
}

//this method is for internal use only
colourPicker.prototype.rebuild = function() {
    this.fillStartColors = new Array(this.rgbArr["fill"]["red"],this.rgbArr["fill"]["green"],this.rgbArr["fill"]["blue"],this.alpha["fill"]);
    this.strokeStartColors = new Array(this.rgbArr["stroke"]["red"],this.rgbArr["stroke"]["green"],this.rgbArr["stroke"]["blue"],this.alpha["stroke"]);
    this.parentGroup.parentNode.removeChild(this.parentGroup);
    this.createColourpicker();
}

//this method allows to move the colourPicker to a different position
colourPicker.prototype.moveTo = function(x,y) {
    this.x = x;
    this.y = y;
    this.rebuild();
}

//this method changes the size of the colourPicker, you should make sure that the width and height values aren't too small, otherwise the colourPicker would look a bit strange ...
colourPicker.prototype.resize = function(width,height) {
    this.width = width;
    this.height = height;
    this.rebuild();
}
