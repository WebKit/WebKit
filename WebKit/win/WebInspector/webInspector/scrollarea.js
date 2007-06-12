/*
© Copyright 2005 Apple Computer, Inc. All rights reserved.

IMPORTANT:  This Apple software and the associated images located in
/System/Library/WidgetResources/AppleClasses/ (collectively "Apple Software")
are supplied to you by Apple Computer, Inc. (“Apple”) in consideration of your
agreement to the following terms. Your use, installation and/or redistribution
of this Apple Software constitutes acceptance of these terms. If you do not
agree with these terms, please do not use, install, or redistribute this Apple
Software.

In consideration of your agreement to abide by the following terms, and subject
to these terms, Apple grants you a personal, non-exclusive license, under
Apple’s copyrights in the Apple Software, to use, reproduce, and redistribute
the Apple Software, in text form (for JavaScript files) or binary form (for
associated images), for the sole purpose of creating Dashboard widgets for Mac
OS X.

If you redistribute the Apple Software, you must retain this entire notice and
the warranty disclaimers and limitation of liability provisions (last two
paragraphs below) in all such redistributions of the Apple Software.

You may not use the name, trademarks, service marks or logos of Apple to endorse
or promote products that include the Apple Software without the prior written
permission of Apple. Except as expressly stated in this notice, no other rights
or licenses, express or implied, are granted by Apple herein, including but not
limited to any patent rights that may be infringed by your products that
incorporate the Apple Software or by other works in which the Apple Software may
be incorporated.

The Apple Software is provided on an "AS IS" basis.  APPLE MAKES NO WARRANTIES,
EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED WARRANTIES OF
NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE,
REGARDING THE APPPLE SOFTWARE OR ITS USE AND OPERATION ALONE OR IN COMBINATION
WITH YOUR PRODUCTS.

IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION, AND/OR DISTRIBUTION OF THE
APPLE SOFTWARE, HOWEVER CAUSED AND WHETHER UNDER THEORY OF CONTRACT, TORT
(INCLUDING NEGLIGENCE), STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN
ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * ScrollArea Constructor
 * content is the element containing the display to be scrolled.
 * Any additional arguments will be added as scrollbars using this.addScrollbar.
 */
function ScrollArea(content)
{
	/* Objects */
	this.content = content;
	
	/* public properties */
	// These are read-write. Set them as needed.
	this.scrollsVertically = true;
	this.scrollsHorizontally = true;
	this.singlepressScrollPixels = 10;
	
	// These are read-only.
	this.viewHeight = 0;
	this.viewToContentHeightRatio = 1.0;
	this.viewWidth = 0;
	this.viewToContentWidthRatio = 1.0;

	/* Internal Objects */
	this._scrollbars = new Array();
	
	// For JavaScript event handlers
	var _self = this;
	
	/*
	 * Privileged methods
	 * These event handlers need to be here because within an event handler,
	 * "this" refers to the element which called the event, rather than the 
	 * class instance.
	 */	
	this._refreshHandler = function() { _self.refresh(); };
	this._keyPressedHandler = function() { _self.keyPressed(event); };
	this._mousewheelScrollHandler = function(event) { _self.mousewheelScroll(event); };

    // Set up the style for the content element just to be certain
	this.content.style.overflow = "hidden";
	this.content.scrollTop = 0;
	this.content.scrollLeft = 0;
	
	// Add event listeners
	this.content.addEventListener("mousewheel", this._mousewheelScrollHandler, true);
	
	this.refresh();
	
	// Add any scrollbars
	var c = arguments.length;
	for (var i = 1; i < c; ++i)
	{
		this.addScrollbar(arguments[i]);
	}
}

ScrollArea.prototype.addScrollbar = function(scrollbar)
{
	scrollbar.setScrollArea(this);
	this._scrollbars.push(scrollbar);
	scrollbar.refresh();
}

ScrollArea.prototype.removeScrollbar = function(scrollbar)
{
	var scrollbars = this._scrollbars;
	var c = scrollbars.length;
	for (var i = 0; i < c; ++i)
	{
		if (scrollbars[i] === scrollbar)
		{
			scrollbars.splice(i, 1);
			break;
		}
	}
}

ScrollArea.prototype.remove = function()
{
	this.content.removeEventListener("mousewheel", this._mousewheelScrollHandler, true);
	
	var scrollbars = this._scrollbars;
	var c = scrollbars.length;
	for (var i = 0; i < c; ++i)
	{
		scrollbars[i].setScrollArea(null);
	}
}

/*
 * refresh() member function
 * Refresh the current scrollbar position and size.
 * This should be called whenever the content element changes.
 */
ScrollArea.prototype.refresh = function()
{	
	// get the current actual view height. Float because we divide.
	var style = document.defaultView.getComputedStyle(this.content, '');
	if (style)
	{
	   this.viewHeight = parseFloat(style.getPropertyValue("height"));
	   this.viewWidth  = parseFloat(style.getPropertyValue("width"));
    }
    else
    {
        this.viewHeight = 0;
        this.viewWidth = 0;
    }
	   
	
	if (this.content.scrollHeight > this.viewHeight)
	{
		this.viewToContentHeightRatio = this.viewHeight / this.content.scrollHeight;
		this.verticalScrollTo(this.content.scrollTop);
	}
	else
	{
		this.viewToContentHeightRatio = 1.0;
		this.verticalScrollTo(0);
	}
	
	if (this.content.scrollWidth > this.viewWidth)
	{
		this.viewToContentWidthRatio = this.viewWidth / this.content.scrollWidth;
		this.horizontalScrollTo(this.content.scrollLeft);
	}
	else
	{
		this.viewToContentWidthRatio = 1.0;
		this.horizontalScrollTo(0);
	}
	
	var scrollbars = this._scrollbars;
	var c = scrollbars.length;
	for (var i = 0; i < c; ++i)
	{
		scrollbars[i].refresh();
	}
}

/*
 * focus() member function.
 * Tell the scrollarea that it is in focus. It will capture keyPressed events
 * and if they are arrow keys scroll accordingly.
 */
ScrollArea.prototype.focus = function()
{
	document.addEventListener("keypress", this._keyPressedHandler, true);
}

/*
 * blur() member function.
 * Tell the scrollarea that it is no longer in focus. It will cease capturing
 * keypress events.
 */
ScrollArea.prototype.blur = function()
{
	document.removeEventListener("keypress", this._keyPressedHandler, true);
}


/*
 * reveal(element) member function.
 * Pass in an Element which is contained within the content element.
 * The content will then be scrolled to reveal that element.
 */
ScrollArea.prototype.reveal = function(element)
{
	var offsetY = 0;
	var obj = element;
	do {
		offsetY += obj.offsetTop;
		obj = obj.offsetParent;
	} while (obj && obj !== this.content);

	var offsetX = 0;
	obj = element;
	do {
		offsetX += obj.offsetLeft;
		obj = obj.offsetParent;
	} while (obj && obj !== this.content);

	var top = this.content.scrollTop;
	var height = this.viewHeight;
	if ((top + height) < (offsetY + element.clientHeight)) 
		this.verticalScrollTo(offsetY - height + element.clientHeight);
	else if (top > offsetY)
		this.verticalScrollTo(offsetY);

	var left = this.content.scrollLeft;
	var width = this.viewWidth;
	if ((left + width) < (offsetX + element.clientWidth)) 
		this.horizontalScrollTo(offsetX - width + element.clientWidth);
	else if (left > offsetX)
		this.horizontalScrollTo(offsetX);
}


ScrollArea.prototype.verticalScrollTo = function(new_content_top)
{
	if (!this.scrollsVertically)
		return;
	
	var bottom = this.content.scrollHeight - this.viewHeight;
	
	if (new_content_top < 0)
	{
		new_content_top = 0;
	}
	else if (new_content_top > bottom)
	{
		new_content_top = bottom;
	}
	
	this.content.scrollTop = new_content_top;
	
	var scrollbars = this._scrollbars;
	var c = scrollbars.length;
	for (var i = 0; i < c; ++i)
	{
		scrollbars[i].verticalHasScrolled();
	}
}

ScrollArea.prototype.horizontalScrollTo = function(new_content_left)
{
	if (!this.scrollsHorizontally)
		return;
	
	var right = this.content_width - this.viewWidth;
	
	if (new_content_left < 0)
	{
		new_content_left = 0;
	}
	else if (new_content_left > right)
	{
		new_content_left = right;
	}
	
	this.content.scrollLeft = new_content_left;
	
	var scrollbars = this._scrollbars;
	var c = scrollbars.length;
	for (var i = 0; i < c; ++i)
	{
		scrollbars[i].horizontalHasScrolled();
	}
}

/*********************
 * Keypressed events
 */
ScrollArea.prototype.keyPressed = function(event)
{
	var handled = true;
	
	if (event.altKey)
		return;
	if (event.shiftKey)
		return;
	
	switch (event.keyIdentifier)
	{
		case "Home":
			this.verticalScrollTo(0);
			break;
		case "End":
			this.verticalScrollTo(this.content.scrollHeight - this.viewHeight);
			break;
		case "Up":
			this.verticalScrollTo(this.content.scrollTop - this.singlepressScrollPixels);
			break;
		case "Down":
			this.verticalScrollTo(this.content.scrollTop + this.singlepressScrollPixels);
			break;
		case "PageUp":
			this.verticalScrollTo(this.content.scrollTop - this.viewHeight);
			break;
		case "PageDown":
			this.verticalScrollTo(this.content.scrollTop + this.viewHeight);
			break;
		case "Left":
			this.horizontalScrollTo(this.content.scrollLeft - this.singlepressScrollPixels);
			break;
		case "Right":
			this.horizontalScrollTo(this.content.scrollLeft + this.singlepressScrollPixels);
			break;
		default:
			handled = false;
	}
	
	if (handled)
	{
		event.stopPropagation();
		event.preventDefault();
	}
}

/*********************
 * Scrollwheel events
 */
ScrollArea.prototype.mousewheelScroll = function(event)
{
	var deltaScroll = event.wheelDelta / 120 * this.singlepressScrollPixels;
	this.verticalScrollTo(this.content.scrollTop - deltaScroll);

	event.stopPropagation();
	event.preventDefault();
}
