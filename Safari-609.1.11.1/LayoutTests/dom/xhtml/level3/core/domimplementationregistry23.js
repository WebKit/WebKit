
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
      return "http://www.w3.org/2001/DOM-Test-Suite/level3/core/domimplementationregistry23";
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
DOMImplementationRegistry.getDOMImplementationList("cOrE 3.0 xMl 3.0 eVeNts 2.0 lS") 
should return an empty list or a list of DOMImplementation that implements the specified features.

* @author Curt Arnold
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/java-binding
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/ecma-script-binding
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#ID-getDOMImpls
*/
function domimplementationregistry23() {
   var success;
    if(checkInitialization(builder, "domimplementationregistry23") != null) return;
    var domImplRegistry;
      var domImpl;
      var hasCore;
      var hasXML;
      var hasEvents;
      var hasLS;
      var baseImpl;
      var nullVersion = null;

      var domImplList;
      var length;
      domImplRegistry = DOMImplementationRegistry;
         assertNotNull("domImplRegistryNotNull",domImplRegistry);
domImplList = domImplRegistry.getDOMImplementationList("cOrE 3.0 xMl 3.0 eVeNts 2.0 lS");
         length = domImplList.length;

      
	if(
	(0 == length)
	) {
	baseImpl = getImplementation();
hasCore = baseImpl.hasFeature("Core","3.0");
hasXML = baseImpl.hasFeature("XML","3.0");
hasEvents = baseImpl.hasFeature("Events","2.0");
hasLS = baseImpl.hasFeature("LS",nullVersion);

			{
			assertFalse("baseImplFeatures",
	(hasCore && hasXML && hasEvents && hasLS)
);

	}
	
		else {
			for(var indexN10096 = 0;indexN10096 < domImplList.length; indexN10096++) {
      domImpl = domImplList.item(indexN10096);
      hasCore = domImpl.hasFeature("Core","3.0");
assertTrue("hasCore",hasCore);
hasXML = domImpl.hasFeature("XML","3.0");
assertTrue("hasXML",hasXML);
hasEvents = domImpl.hasFeature("Events","2.0");
assertTrue("hasEvents",hasEvents);
hasLS = domImpl.hasFeature("LS",nullVersion);
assertTrue("hasLS",hasLS);

	}
   
		}
	
}




function runTest() {
   domimplementationregistry23();
}
