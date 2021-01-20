
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
      return "http://www.w3.org/2001/DOM-Test-Suite/level3/core/documentadoptnode32";
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
        
      var docAdopterRef = null;
      if (typeof(this.docAdopter) != 'undefined') {
        docAdopterRef = this.docAdopter;
      }
      docsLoaded += preload(docAdopterRef, "docAdopter", "hc_staff");
        
       if (docsLoaded == 2) {
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
    if (++docsLoaded == 2) {
        setUpPageStatus = 'complete';
    }
}


/**
* 
	Invoke the adoptNode method on another document using a new CDataSection node created in this
	Document as the source.  Verify if the node has been adopted correctly by checking the nodeValue 
	of the adopted node.

* @author IBM
* @author Neil Delima
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#Document3-adoptNode
*/
function documentadoptnode32() {
   var success;
    if(checkInitialization(builder, "documentadoptnode32") != null) return;
    var doc;
      var docAdopter;
      var newCDATA;
      var adoptedCDATA;
      var nodeValue;
      
      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      doc = load(docRef, "doc", "hc_staff");
      
      var docAdopterRef = null;
      if (typeof(this.docAdopter) != 'undefined') {
        docAdopterRef = this.docAdopter;
      }
      docAdopter = load(docAdopterRef, "docAdopter", "hc_staff");
      newCDATA = doc.createCDATASection("Document.adoptNode test for a CDATASECTION_NODE");
      adoptedCDATA = docAdopter.adoptNode(newCDATA);
      
	if(
	
	(adoptedCDATA != null)

	) {
	nodeValue = adoptedCDATA.nodeValue;

      assertEquals("documentadoptnode32","Document.adoptNode test for a CDATASECTION_NODE",nodeValue);
       
	}
	
}




function runTest() {
   documentadoptnode32();
}
