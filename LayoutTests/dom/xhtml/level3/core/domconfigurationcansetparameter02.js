
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
      return "http://www.w3.org/2001/DOM-Test-Suite/level3/core/domconfigurationcansetparameter02";
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
Check that canSetParameter('cdata-sections') returns true for both true and false
and that calls to the method do not actually change the parameter value.

* @author IBM
* @author Jenny Hsu
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#DOMConfiguration-canSetParameter
*/
function domconfigurationcansetparameter02() {
   var success;
    if(checkInitialization(builder, "domconfigurationcansetparameter02") != null) return;
    var doc;
      var domConfig;
      var canSet;
      var paramVal;
      
      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      doc = load(docRef, "doc", "hc_staff");
      domConfig = doc.domConfig;

      canSet = domConfig.canSetParameter("cdata-sections",false);
      assertTrue("canSetFalse",canSet);
paramVal = domConfig.getParameter("cdata-sections");
      assertTrue("valueStillTrue",paramVal);
canSet = domConfig.canSetParameter("cdata-sections",true);
      assertTrue("canSetTrue",canSet);
domConfig.setParameter("cdata-sections", false);
	 canSet = domConfig.canSetParameter("cdata-sections",true);
      assertTrue("canSetTrueFromFalse",canSet);
paramVal = domConfig.getParameter("cdata-sections");
      assertFalse("valueStillFalse",paramVal);
canSet = domConfig.canSetParameter("cdata-sections",false);
      assertTrue("canSetFalseFromFalse",canSet);

}




function runTest() {
   domconfigurationcansetparameter02();
}
