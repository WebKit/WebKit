
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
return ['XPathNSResolver_lookupNamespaceURI_null'];
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
          setUpPage = 'complete';
       }
    } catch(ex) {
    	catchInitializationError(builder, ex);
        setUpPage = 'complete';
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
      Iterate over all elements in the test document, creating
      nsresolvers checking that looking up non-existent prefixes
      always returns null.
    
* @author Bob Clary
* @see http://www.w3.org/TR/2003/CR-DOM-Level-3-XPath-20030331/xpath#XPathEvaluator
* @see http://www.w3.org/TR/2003/CR-DOM-Level-3-XPath-20030331/xpath#XPathEvaluator-createNSResolver
* @see http://www.w3.org/TR/2003/CR-DOM-Level-3-XPath-20030331/xpath#XPathNSResolver
* @see http://www.w3.org/TR/2003/CR-DOM-Level-3-XPath-20030331/xpath#XPathNSResolver-lookupNamespaceURI
*/
function XPathNSResolver_lookupNamespaceURI_null() {
   var success;
    if(checkInitialization(builder, "XPathNSResolver_lookupNamespaceURI_null") != null) return;
    var doc;
      var resolver;
      var evaluator;
      var element;
      var elements;
      var lookupNamespaceURI;
      var namespaceURI;
      var prefix;
      
      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      doc = load(docRef, "doc", "staffNS");
      elements = doc.getElementsByTagNameNS("*","*");
      evaluator = createXPathEvaluator(doc);
for(var indexN65631 = 0;indexN65631 < elements.length; indexN65631++) {
      element = elements.item(indexN65631);
      resolver = evaluator.createNSResolver(element);
      lookupNamespaceURI = resolver.lookupNamespaceURI("foobar");
      assertNull("notnull",lookupNamespaceURI);
    
	}
   
}




function runTest() {
   XPathNSResolver_lookupNamespaceURI_null();
}
