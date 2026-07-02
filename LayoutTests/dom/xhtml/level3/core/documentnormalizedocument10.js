
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
      return "http://www.w3.org/2001/DOM-Test-Suite/level3/core/documentnormalizedocument10";
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
	The normalizeDocument method method acts as if the document was going through a save 
	and load cycle, putting the document in a "normal" form. 

	Create an Element and a text node and verify the nodeValue of this text node and append these to
	this Document.  If supported, invoke the setParameter method on this domconfiguration object to set the 
	"element-content-whitespace"  feature to false.  Invoke the normalizeDocument method and verify if 
	the text node has been discarded.

* @author IBM
* @author Neil Delima
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#Document3-normalizeDocument
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#parameter-element-content-whitespace
*/
function documentnormalizedocument10() {
   var success;
    if(checkInitialization(builder, "documentnormalizedocument10") != null) return;
    var doc;
      var elem;
      var newText;
      var text;
      var nodeValue;
      var canSet;
      var appendedChild;
      var domConfig;
      
      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      doc = load(docRef, "doc", "hc_staff");
      elem = doc.createElement("newElem");
      newText = doc.createTextNode("Text          Node");
      appendedChild = elem.appendChild(newText);
      appendedChild = doc.appendChild(elem);
      text = elem.firstChild;

      nodeValue = text.nodeValue;

      assertEquals("documentnormalizedocument10","Text          Node",nodeValue);
       domConfig = doc.domConfig;

      canSet = domConfig.canSetParameter("element-content-whitespace",true);
      assertTrue("canSetElementContentWhitespaceTrue",canSet);
domConfig.setParameter("element-content-whitespace", true);
	 doc.normalizeDocument();
      text = elem.firstChild;

      nodeValue = text.nodeValue;

      assertEquals("documentnormalizedocument10_true1","Text          Node",nodeValue);
       canSet = domConfig.canSetParameter("element-content-whitespace",false);
      
	if(
	canSet
	) {
	domConfig.setParameter("element-content-whitespace", false);
	 doc.normalizeDocument();
      text = elem.firstChild;

      nodeValue = text.nodeValue;

      assertEquals("documentnormalizedocument10_true2","Text Node",nodeValue);
       
	}
	
}




function runTest() {
   documentnormalizedocument10();
}
