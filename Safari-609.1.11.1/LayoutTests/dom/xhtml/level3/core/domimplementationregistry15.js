
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
      return "http://www.w3.org/2001/DOM-Test-Suite/level3/core/domimplementationregistry15";
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
* 
DOMImplementationRegistry.getDOMImplementationList("+cOrE") should return 
list of DOMImplementation
where hasFeature("+Core", null) returns true.

* @author Curt Arnold
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/java-binding
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/ecma-script-binding
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#ID-getDOMImpls
*/
function domimplementationregistry15() {
   var success;
    if(checkInitialization(builder, "domimplementationregistry15") != null) return;
    var domImplRegistry;
      var domImpl;
      var hasFeature;
      var nullVersion = null;

      var domImplList;
      var length;
      domImplRegistry = DOMImplementationRegistry;
         assertNotNull("domImplRegistryNotNull",domImplRegistry);
domImplList = domImplRegistry.getDOMImplementationList("+cOrE");
         length = domImplList.length;

      	assertTrue("atLeastOne",
      
	(length > 0)
);
for(var indexN10057 = 0;indexN10057 < domImplList.length; indexN10057++) {
      domImpl = domImplList.item(indexN10057);
      hasFeature = domImpl.hasFeature("+Core",nullVersion);
assertTrue("hasCore",hasFeature);

	}
   
}




function runTest() {
   domimplementationregistry15();
}
