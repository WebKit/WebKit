
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
      return "http://www.w3.org/2001/DOM-Test-Suite/level3/core/documentnormalizedocument11";
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
       setImplementationAttribute("ignoringElementContentWhitespace", true);

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
	The feature namespace-declarations when set to false, discards all namespace declaration attributes,
        although namespace prefixes are still retained.
	
	Set the normalization feature "namespace-declarations" to false, invoke normalizeDocument and verify 
        the nodeName of element acquired by tagname.  

* @author IBM
* @author Neil Delima
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#Document3-normalizeDocument
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#parameter-namespace-declarations
*/
function documentnormalizedocument11() {
   var success;
    if(checkInitialization(builder, "documentnormalizedocument11") != null) return;
    var doc;
      var elemList;
      var elemName;
      var nodeName;
      var canSet;
      var domConfig;
      
      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      doc = load(docRef, "doc", "hc_staff");
      domConfig = doc.domConfig;

      domConfig.setParameter("namespace-declarations", true);
	 doc.normalizeDocument();
      elemList = doc.getElementsByTagNameNS("*","acronym");
      elemName = elemList.item(1);
      assertNotNull("documentnormalizedocument11_NotNullElem",elemName);
canSet = domConfig.canSetParameter("namespace-declarations",false);
      
	if(
	canSet
	) {
	domConfig.setParameter("namespace-declarations", false);
	 doc.normalizeDocument();
      elemList = doc.getElementsByTagNameNS("*","acronym");
      elemName = elemList.item(1);
      nodeName = elemName.nodeName;

      assertEquals("documentnormalizedocument11_namespaceDeclarations","address",nodeName);
       
	}
	
}




function runTest() {
   documentnormalizedocument11();
}
