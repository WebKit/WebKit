
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
      return "http://www.w3.org/2001/DOM-Test-Suite/level3/core/domconfigdatatypenormalization1";
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
* Checks behavior of "datatype-normalization" configuration parameter.
* @author Curt Arnold
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#parameter-datatype-normalization
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#DOMConfiguration
*/
function domconfigdatatypenormalization1() {
   var success;
    if(checkInitialization(builder, "domconfigdatatypenormalization1") != null) return;
    var domImpl;
      var doc;
      var domConfig;
      var nullDocType = null;

      var canSet;
      var state;
      var parameter = "dAtAtype-normalization";
      domImpl = getImplementation();
doc = domImpl.createDocument("http://www.w3.org/1999/xhtml","html",nullDocType);
      domConfig = doc.domConfig;

      state = domConfig.getParameter(parameter);
      assertFalse("defaultFalse",state);
canSet = domConfig.canSetParameter(parameter,false);
      assertTrue("canSetFalse",canSet);
canSet = domConfig.canSetParameter(parameter,true);
      
	if(
	canSet
	) {
	domConfig.setParameter(parameter, true);
	 state = domConfig.getParameter(parameter);
      assertTrue("setTrueEffective",state);

	}
	
		else {
			
	{
		success = false;
		try {
            domConfig.setParameter(parameter, true);
	   }
		catch(ex) {
      success = (typeof(ex.code) != 'undefined' && ex.code == 9);
		}
		assertTrue("throw_NOT_SUPPORTED_ERR",success);
	}
state = domConfig.getParameter(parameter);
      assertFalse("setTrueNotEffective",state);

		}
	domConfig.setParameter(parameter, false);
	 
}




function runTest() {
   domconfigdatatypenormalization1();
}
