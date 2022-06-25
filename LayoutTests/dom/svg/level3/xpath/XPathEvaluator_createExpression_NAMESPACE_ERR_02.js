
/*
Copyright Â© 2001-2004 World Wide Web Consortium, 
(Massachusetts Institute of Technology, European Research Consortium 
for Informatics and Mathematics, Keio University). All 
Rights Reserved. This work is distributed under the W3CÂ® Software License [1] in the 
hope that it will be useful, but WITHOUT ANY WARRANTY; without even 
the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 

[1] http://www.w3.org/Consortium/Legal/2002/copyright-software-20021231
*/


// expose test function names
function exposeTestFunctionNames()
{
return ['XPathEvaluator_createExpression_NAMESPACE_ERR_02'];
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
      docsLoaded += preload(docRef, "doc", "staffNS");
        
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
      The XPathEvaluator can create a "XPathExpression" using the method
      "createExpression(expression, resolver)".
      
      Retrieve the DOM document on which the
      'createExpression("/staff/jfouffa:employee", resolver)' method is
      invoked with the document element. The method should fail to create
      pre-compiled expression and throws DOMException.NAMESPACE_ERR
      since the prefix jfouffa is not mapped.
    
* @author Philippe Le Hégaret
* @see http://www.w3.org/TR/2003/CR-DOM-Level-3-XPath-20030331/xpath#XPathEvaluator-createExpression
*/
function XPathEvaluator_createExpression_NAMESPACE_ERR_02() {
   var success;
    if(checkInitialization(builder, "XPathEvaluator_createExpression_NAMESPACE_ERR_02") != null) return;
    var doc;
      var xpEvaluator;
      var exp;
      var root;
      var resolver;
      
      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      doc = load(docRef, "doc", "staffNS");
      xpEvaluator = createXPathEvaluator(doc);
root = doc.documentElement;

      resolver = xpEvaluator.createNSResolver(root);
      
	{
		success = false;
		try {
            exp = xpEvaluator.createExpression("/staff/jfouffa:employee",resolver);
        }
		catch(ex) {
      success = (typeof(ex.code) != 'undefined' && ex.code == 14);
		}
		assertTrue("throw_NAMESPACE_ERR",success);
	}

}




function runTest() {
   XPathEvaluator_createExpression_NAMESPACE_ERR_02();
}
