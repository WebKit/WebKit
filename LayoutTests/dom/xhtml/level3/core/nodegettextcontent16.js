
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
      return "http://www.w3.org/2001/DOM-Test-Suite/level3/core/nodegettextcontent16";
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
       setImplementationAttribute("namespaceAware", true);
       setImplementationAttribute("expandEntityReferences", true);

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
	The method getTextContent returns the text content of this node and its descendants.
	
	Invoke the method getTextContent on a new DocumentFragment node with new Text, EntityReferences  
	CDATASection, PI and Comment nodes and check if the value returned is a single 
	concatenated String with its content.  

* @author IBM
* @author Neil Delima
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#Node3-textContent
*/
function nodegettextcontent16() {
   var success;
    if(checkInitialization(builder, "nodegettextcontent16") != null) return;
    var doc;
      var docFrag;
      var elem;
      var elemChild;
      var txt;
      var comment;
      var entRef;
      var cdata;
      var pi;
      var textContent;
      var appendedChild;
      
      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      doc = load(docRef, "doc", "hc_staff");
      docFrag = doc.createDocumentFragment();
      elem = doc.createElementNS("http://www.w3.org/DOM/Test","dom3:elem");
      txt = doc.createTextNode("Text ");
      comment = doc.createComment("Comment ");
      entRef = doc.createEntityReference("beta");
      pi = doc.createProcessingInstruction("PIT","PIData ");
      cdata = doc.createCDATASection("CData");
      appendedChild = elem.appendChild(txt);
      appendedChild = elem.appendChild(comment);
      appendedChild = elem.appendChild(entRef);
      appendedChild = elem.appendChild(pi);
      appendedChild = elem.appendChild(cdata);
      appendedChild = docFrag.appendChild(elem);
      doc.normalizeDocument();
      textContent = docFrag.textContent;

      assertEquals("nodegettextcontent16","Text βCData",textContent);
       
}




function runTest() {
   nodegettextcontent16();
}
