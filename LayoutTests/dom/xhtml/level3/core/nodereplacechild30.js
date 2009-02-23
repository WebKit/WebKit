
/*
Copyright Â© 2001-2004 World Wide Web Consortium, 
(Massachusetts Institute of Technology, European Research Consortium 
for Informatics and Mathematics, Keio University). All 
Rights Reserved. This work is distributed under the W3CÂ® Software License [1] in the 
hope that it will be useful, but WITHOUT ANY WARRANTY; without even 
the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 

[1] http://www.w3.org/Consortium/Legal/2002/copyright-software-20021231
*/



   /**
    *  Gets URI that identifies the test.
    *  @return uri identifier of test
    */
function getTargetURI() {
      return "http://www.w3.org/2001/DOM-Test-Suite/level3/core/nodereplacechild30";
   }

var docsLoaded = -1000000;
var builder = null;

//
//   This function is called by the testing framework before
//      running the test suite.
//
//   If there are no configuration exceptions, asynchronous
//        document loading is started.  Otherwise, the status
//        is set to complete and the exception is immediately
//        raised when entering the body of the test.
//
function setUpPage() {
   setUpPageStatus = 'running';
   try {
     //
     //   creates test document builder, may throw exception
     //
     builder = createConfiguredBuilder();

      docsLoaded = 0;
      
      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      docsLoaded += preload(docRef, "doc", "hc_staff");
        
       if (docsLoaded == 1) {
          setUpPageStatus = 'complete';
       }
    } catch(ex) {
    	catchInitializationError(builder, ex);
        setUpPageStatus = 'complete';
    }
}



//
//   This method is called on the completion of 
//      each asychronous load started in setUpTests.
//
//   When every synchronous loaded document has completed,
//      the page status is changed which allows the
//      body of the test to be executed.
function loadComplete() {
    if (++docsLoaded == 1) {
        setUpPageStatus = 'complete';
    }
}


/**
* 



	Using replaceChild on an Element node attempt to replace a new Element child node with 
	new child nodes and vice versa and in each case verify the name of the replaced node.

* @author IBM
* @author Neil Delima
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#ID-785887307
*/
function nodereplacechild30() {
   var success;
    if(checkInitialization(builder, "nodereplacechild30") != null) return;
    var doc;
      var parent;
      var oldChild;
      var newElement;
      var newText;
      var newComment;
      var newPI;
      var newCdata;
      var newERef;
      var replaced;
      var nodeName;
      var appendedChild;
      
      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      doc = load(docRef, "doc", "hc_staff");
      parent = doc.createElementNS("http://www.w3.org/1999/xhtml","xhtml:html");
      oldChild = doc.createElementNS("http://www.w3.org/1999/xhtml","xhtml:head");
      newElement = doc.createElementNS("http://www.w3.org/1999/xhtml","xhtml:body");
      appendedChild = parent.appendChild(oldChild);
      appendedChild = parent.appendChild(newElement);
      newText = doc.createTextNode("Text");
      appendedChild = parent.appendChild(newText);
      newComment = doc.createComment("Comment");
      appendedChild = parent.appendChild(newComment);
      newPI = doc.createProcessingInstruction("target","data");
      appendedChild = parent.appendChild(newPI);
      newCdata = doc.createCDATASection("Cdata");
      appendedChild = parent.appendChild(newCdata);
      newERef = doc.createEntityReference("delta");
      appendedChild = parent.appendChild(newERef);
      replaced = parent.replaceChild(newElement,oldChild);
      nodeName = replaced.nodeName;

      assertEquals("nodereplacechild30_1","xhtml:head",nodeName);
       replaced = parent.replaceChild(oldChild,newElement);
      nodeName = replaced.nodeName;

      assertEquals("nodereplacechild30_2","xhtml:body",nodeName);
       replaced = parent.replaceChild(newText,oldChild);
      nodeName = replaced.nodeName;

      assertEquals("nodereplacechild30_3","xhtml:head",nodeName);
       replaced = parent.replaceChild(oldChild,newText);
      nodeName = replaced.nodeName;

      assertEquals("nodereplacechild30_4","#text",nodeName);
       replaced = parent.replaceChild(newComment,oldChild);
      nodeName = replaced.nodeName;

      assertEquals("nodereplacechild30_5","xhtml:head",nodeName);
       replaced = parent.replaceChild(oldChild,newComment);
      nodeName = replaced.nodeName;

      assertEquals("nodereplacechild30_6","#comment",nodeName);
       replaced = parent.replaceChild(oldChild,newPI);
      nodeName = replaced.nodeName;

      assertEquals("nodereplacechild30_7","target",nodeName);
       replaced = parent.replaceChild(oldChild,newCdata);
      nodeName = replaced.nodeName;

      assertEquals("nodereplacechild30_8","#cdata-section",nodeName);
       replaced = parent.replaceChild(oldChild,newERef);
      nodeName = replaced.nodeName;

      assertEquals("nodereplacechild30_9","delta",nodeName);
       
}




function runTest() {
   nodereplacechild30();
}
