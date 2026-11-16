
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
      return "http://www.w3.org/2001/DOM-Test-Suite/level3/core/domconfigerrorhandler1";
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
      
       if (docsLoaded == 0) {
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
    if (++docsLoaded == 0) {
        setUpPageStatus = 'complete';
    }
}


     /**
      *    Inner class implementation for variable errorHandler 
      */
var errorHandler;

/**
        * Constructor

        */
	      
function DOMErrorHandlerN10049() { 
           }
   
        /**
         *    
This method is called on the error handler when an error occurs.
If an exception is thrown from this method, it is considered to be equivalent of returningtrue.

         * @param error 
The error object that describes the error. This object may be reused by the DOM implementation across multiple calls to thehandleErrormethod.

         */
DOMErrorHandlerN10049.prototype.handleError = function(error) {
         //
         //   bring class variables into function scope
         //
                return true;
}

/**
* Checks behavior of "error-handler" configuration parameter.
* @author Curt Arnold
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#parameter-error-handler
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#DOMConfiguration-getParameter
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#DOMConfiguration-setParameter
* @see http://www.w3.org/Bugs/Public/show_bug.cgi?id=544
*/
function domconfigerrorhandler1() {
   var success;
    if(checkInitialization(builder, "domconfigerrorhandler1") != null) return;
    var domImpl;
      var doc;
      var domConfig;
      var nullDocType = null;

      var canSet;
      var origHandler;
      var state;
      var parameter = "eRrOr-handler";
      errorHandler = new DOMErrorHandlerN10049();
	  
      domImpl = getImplementation();
doc = domImpl.createDocument("http://www.w3.org/1999/xhtml","html",nullDocType);
      domConfig = doc.domConfig;

      origHandler = domConfig.getParameter(parameter);
      canSet = domConfig.canSetParameter(parameter,errorHandler);
      assertTrue("canSetNewHandler",canSet);
canSet = domConfig.canSetParameter(parameter,origHandler);
      assertTrue("canSetOrigHandler",canSet);
domConfig.setParameter(parameter, errorHandler.handleError);
	 state = domConfig.getParameter(parameter);
      assertSame("setToNewHandlerEffective",errorHandler,state);
domConfig.setParameter(parameter, origHandler.handleError);
	 state = domConfig.getParameter(parameter);
      assertSame("setToOrigHandlerEffective",origHandler,state);
canSet = domConfig.canSetParameter(parameter,true);
      
	if(
	canSet
	) {
	domConfig.setParameter(parameter, true);
	 
	}
	
}




function runTest() {
   domconfigerrorhandler1();
}
