
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
      return "http://www.w3.org/2001/DOM-Test-Suite/level3/core/domconfigparameternames01";
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
* Checks getParameterNames and canSetParameter for Document.domConfig.
* @author Curt Arnold
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#Document3-domConfig
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#DOMConfiguration-parameterNames
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#parameter-canonical-form
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#parameter-cdata-sections
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#parameter-check-character-normalization
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#parameter-comments
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#parameter-datatype-normalization
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#parameter-entities
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#parameter-error-handler
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#parameter-infoset
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#parameter-namespaces
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#parameter-namespace-declarations
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#parameter-normalize-characters
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#parameter-split-cdata-sections
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#parameter-validate
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#parameter-validate-if-schema
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#parameter-well-formed
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#parameter-element-content-whitespace
*/
function domconfigparameternames01() {
   var success;
    if(checkInitialization(builder, "domconfigparameternames01") != null) return;
    var domImpl;
      var doc;
      var config;
      var state;
      var parameterNames;
      var parameterName;
      var matchCount = 0;
      var paramValue;
      var canSet;
      
      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      doc = load(docRef, "doc", "barfoo");
      config = doc.domConfig;

      assertNotNull("configNotNull",config);
parameterNames = config.parameterNames;

      assertNotNull("parameterNamesNotNull",parameterNames);
for(var indexN1008C = 0;indexN1008C < parameterNames.length; indexN1008C++) {
      parameterName = parameterNames.item(indexN1008C);
      paramValue = config.getParameter(parameterName);
      canSet = config.canSetParameter(parameterName,paramValue);
      assertTrue("canSetToDefaultValue",canSet);
config.setParameter(parameterName, paramValue);
	 
	if(
	
	(("canonical-form".toUpperCase() == parameterName.toUpperCase()) || ("cdata-sections".toUpperCase() == parameterName.toUpperCase()) || ("check-character-normalization".toUpperCase() == parameterName.toUpperCase()) || ("comments".toUpperCase() == parameterName.toUpperCase()) || ("datatype-normalization".toUpperCase() == parameterName.toUpperCase()) || ("entities".toUpperCase() == parameterName.toUpperCase()) || ("error-handler".toUpperCase() == parameterName.toUpperCase()) || ("infoset".toUpperCase() == parameterName.toUpperCase()) || ("namespaces".toUpperCase() == parameterName.toUpperCase()) || ("namespace-declarations".toUpperCase() == parameterName.toUpperCase()) || ("normalize-characters".toUpperCase() == parameterName.toUpperCase()) || ("split-cdata-sections".toUpperCase() == parameterName.toUpperCase()) || ("validate".toUpperCase() == parameterName.toUpperCase()) || ("validate-if-schema".toUpperCase() == parameterName.toUpperCase()) || ("well-formed".toUpperCase() == parameterName.toUpperCase()) || ("element-content-whitespace".toUpperCase() == parameterName.toUpperCase()))

	) {
	matchCount += 1;

	}
	
	}
   assertEquals("definedParameterCount",16,matchCount);
       
}




function runTest() {
   domconfigparameternames01();
}
