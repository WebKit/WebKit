
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
      return "http://www.w3.org/2001/DOM-Test-Suite/level3/core/domconfigurationcansetparameter06";
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
* 
Check that canSetParameter('element-content-whitespace', true) returns true
and that canSetParameter('element-content-whitespace) does not change value of
parameter.

* @author IBM
* @author Jenny Hsu
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#DOMConfiguration-canSetParameter
*/
function domconfigurationcansetparameter06() {
   var success;
    if(checkInitialization(builder, "domconfigurationcansetparameter06") != null) return;
    var doc;
      var domConfig;
      var itemList;
      var elementBody;
      var textNode;
      var canSet;
      var canSetFalse;
      var paramVal;
      var hasWhitespace;
      
      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      doc = load(docRef, "doc", "barfoo");
      domConfig = doc.domConfig;

      canSetFalse = domConfig.canSetParameter("element-content-whitespace",false);
      paramVal = domConfig.getParameter("element-content-whitespace");
      assertTrue("stillTrue",paramVal);

	if(
	canSetFalse
	) {
	domConfig.setParameter("element-content-whitespace", false);
	 
	}
	canSet = domConfig.canSetParameter("element-content-whitespace",true);
      assertTrue("canSetTrue",canSet);

	if(
	canSetFalse
	) {
	paramVal = domConfig.getParameter("element-content-whitespace");
      assertFalse("stillFalse",paramVal);

	}
	
}




function runTest() {
   domconfigurationcansetparameter06();
}
