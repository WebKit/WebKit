
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
      return "http://www.w3.org/2001/DOM-Test-Suite/level3/core/handleerror02";
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
      docsLoaded += preload(docRef, "doc", "barfoo");
        
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
      *    Inner class implementation for variable errorHandler 
      */
var errorHandler;

/**
        * Constructor

        * @param errors Value from value attribute of nested var element
        */
	      
function DOMErrorHandlerN10053(errors) { 
           this.errors = errors;
           }
   
        /**
         *    
This method is called on the error handler when an error occurs.
If an exception is thrown from this method, it is considered to be equivalent of returningtrue.

         * @param error 
The error object that describes the error. This object may be reused by the DOM implementation across multiple calls to thehandleErrormethod.

         */
DOMErrorHandlerN10053.prototype.handleError = function(error) {
         //
         //   bring class variables into function scope
         //
        var errors = errorHandler.errors;
           var severity;
      severity = error.severity;

      
	if(
	(2 == severity)
	) {
	errors[errors.length] = error;

	}
	        return true;
}

/**
* 
Normalize document with two DOM L1 nodes.
Use an error handler to continue from errors and check that more than one
error was reported.

* @author Curt Arnold
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#Document3-normalizeDocument
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#parameter-namespaces
* @see http://www.w3.org/TR/2003/WD-charmod-20030822/
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#ID-ERRORS-DOMErrorHandler-handleError
*/
function handleerror02() {
   var success;
    if(checkInitialization(builder, "handleerror02") != null) return;
    var doc;
      var docElem;
      var domConfig;
      var pList;
      var pElem;
      var text;
      var textValue;
      var retval;
      var brElem;
      var errors = new Array();

      errorHandler = new DOMErrorHandlerN10053(errors);
	  
      
      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      doc = load(docRef, "doc", "barfoo");
      domConfig = doc.domConfig;

      domConfig.setParameter("error-handler", errorHandler.handleError);
	 pList = doc.getElementsByTagName("p");
      pElem = pList.item(0);
      brElem = doc.createElement("br");
      retval = pElem.appendChild(brElem);
      brElem = doc.createElement("br");
      retval = pElem.appendChild(brElem);
      doc.normalizeDocument();
      assertSize("twoErrors",2,errors);

}




function runTest() {
   handleerror02();
}
