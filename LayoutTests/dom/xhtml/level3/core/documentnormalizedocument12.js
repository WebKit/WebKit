
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
      return "http://www.w3.org/2001/DOM-Test-Suite/level3/core/documentnormalizedocument12";
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
       setImplementationAttribute("validating", true);
       setImplementationAttribute("schemaValidating", true);
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
      *    Inner class implementation for variable errHandler 
      */
var errHandler;

/**
        * Constructor

        */
	      
function DOMErrorHandlerN10048() { 
           }
   
        /**
         *    
This method is called on the error handler when an error occurs.
If an exception is thrown from this method, it is considered to be equivalent of returningtrue.

         * @param error 
The error object that describes the error. This object may be reused by the DOM implementation across multiple calls to thehandleErrormethod.

         */
DOMErrorHandlerN10048.prototype.handleError = function(error) {
         //
         //   bring class variables into function scope
         //
        assertFalse("documentnormalizedocument08_Err",true);
        return true;
}

/**
* 
	The normalizeDocument method method acts as if the document was going through a save 
	and load cycle, putting the document in a "normal" form. 

	Set the validate feature to true.  Invoke the normalizeDocument method on this 
	document.  Retreive the documentElement node and check the nodeName of this node 
	to make sure it has not changed.  Now set validate to false and verify the same. 
	Register an error handler on this Document and in each case make sure that it does
	not get called.

* @author IBM
* @author Neil Delima
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#Document3-normalizeDocument
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#parameter-validate
*/
function documentnormalizedocument12() {
   var success;
    if(checkInitialization(builder, "documentnormalizedocument12") != null) return;
    var doc;
      var docElem;
      var docElemNodeName;
      var canSet;
      var domConfig;
      var errorHandler;
      errHandler = new DOMErrorHandlerN10048();
	  
      
      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      doc = load(docRef, "doc", "hc_staff");
      domConfig = doc.domConfig;

      domConfig.setParameter("error-handler", errHandler.handleError);
	 canSet = domConfig.canSetParameter("validate",true);
      
	if(
	canSet
	) {
	domConfig.setParameter("validate", true);
	 doc.normalizeDocument();
      docElem = doc.documentElement;

      docElemNodeName = docElem.nodeName;

      assertEquals("documentnormalizedocument08_True","html",docElemNodeName);
       
	}
	domConfig.setParameter("validate", false);
	 doc.normalizeDocument();
      docElem = doc.documentElement;

      docElemNodeName = docElem.nodeName;

      assertEquals("documentnormalizedocument08_False","html",docElemNodeName);
       
}




function runTest() {
   documentnormalizedocument12();
}
