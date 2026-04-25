
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
      return "http://www.w3.org/2001/DOM-Test-Suite/level3/core/handleerror01";
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

        */
	      
function DOMErrorHandlerN10054() { 
           }
   
        /**
         *    
This method is called on the error handler when an error occurs.
If an exception is thrown from this method, it is considered to be equivalent of returningtrue.

         * @param error 
The error object that describes the error. This object may be reused by the DOM implementation across multiple calls to thehandleErrormethod.

         */
DOMErrorHandlerN10054.prototype.handleError = function(error) {
         //
         //   bring class variables into function scope
         //
                return false;
}

/**
* 
Add two CDATASection containing "]]>" and call Node.normalize
with an error handler that stops processing.  Only one of the
CDATASections should be split.

* @author Curt Arnold
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#ID-normalize
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#parameter-split-cdata-sections
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#ID-ERRORS-DOMErrorHandler-handleError
*/
function handleerror01() {
   var success;
    if(checkInitialization(builder, "handleerror01") != null) return;
    var doc;
      var elem;
      var domConfig;
      var elemList;
      var newChild;
      var oldChild;
      var child;
      var childValue;
      var childType;
      var retval;
      var errors = new Array();

      errorHandler = new DOMErrorHandlerN10054();
	  
      
      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      doc = load(docRef, "doc", "barfoo");
      elemList = doc.getElementsByTagName("p");
      elem = elemList.item(0);
      oldChild = elem.firstChild;

      newChild = doc.createCDATASection("this is not ]]> good");
      retval = elem.replaceChild(newChild,oldChild);
      newChild = doc.createCDATASection("this is not ]]> bad");
      retval = elem.appendChild(newChild);
      domConfig = doc.domConfig;

      domConfig.setParameter("split-cdata-sections", true);
	 domConfig.setParameter("error-handler", errorHandler.handleError);
	 doc.normalizeDocument();
      elemList = doc.getElementsByTagName("p");
      elem = elemList.item(0);
      child = elem.lastChild;

      childValue = child.nodeValue;

      
	if(
	("this is not ]]> bad" == childValue)
	) {
	childType = child.nodeType;

      assertEquals("lastChildCDATA",4,childType);
       child = elem.firstChild;

      childValue = child.nodeValue;

      assert("firstChildNotIntact","this is not ]]> good" != childValue);
      
	}
	
		else {
			child = elem.firstChild;

      childValue = child.nodeValue;

      assertEquals("firstChildIntact","this is not ]]> good",childValue);
       
		}
	
}




function runTest() {
   handleerror01();
}
