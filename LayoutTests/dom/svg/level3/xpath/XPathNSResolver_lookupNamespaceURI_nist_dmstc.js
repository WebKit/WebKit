
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
return ['XPathNSResolver_lookupNamespaceURI_nist_dmstc'];
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
      Interate over all employee elements with xmlns:dmstc attribute
      in the test document, creating nsresolvers checking that 
      for all children the prefix 'nist' resolves to 
      http://www.nist.gov and that prefix 'dmstc' resolves to the same 
      value as employee.getAttribute('xmlns:dmstc').
    
* @author Bob Clary
* @see http://www.w3.org/TR/2003/CR-DOM-Level-3-XPath-20030331/xpath#XPathEvaluator
* @see http://www.w3.org/TR/2003/CR-DOM-Level-3-XPath-20030331/xpath#XPathEvaluator-createNSResolver
* @see http://www.w3.org/TR/2003/CR-DOM-Level-3-XPath-20030331/xpath#XPathNSResolver
* @see http://www.w3.org/TR/2003/CR-DOM-Level-3-XPath-20030331/xpath#XPathNSResolver-lookupNamespaceURI
*/
function XPathNSResolver_lookupNamespaceURI_nist_dmstc() {
   var success;
    if(checkInitialization(builder, "XPathNSResolver_lookupNamespaceURI_nist_dmstc") != null) return;
    var doc;
      var resolver;
      var evaluator;
      var lookupNamespaceURI;
      var namespaceURI;
      var child;
      var children;
      var employee;
      var employees;
      
      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      doc = load(docRef, "doc", "staffNS");
      employees = doc.getElementsByTagNameNS("*","employee");
      evaluator = createXPathEvaluator(doc);
for(var indexN65633 = 0;indexN65633 < employees.length; indexN65633++) {
      employee = employees.item(indexN65633);
      namespaceURI = employee.getAttribute("xmlns:dmstc");
      children = employee.getElementsByTagNameNS("*","*");
      for(var indexN65650 = 0;indexN65650 < children.length; indexN65650++) {
      child = children.item(indexN65650);
      resolver = evaluator.createNSResolver(child);
      lookupNamespaceURI = resolver.lookupNamespaceURI("dmstc");
      assertEquals("dmstcequal",namespaceURI,lookupNamespaceURI);
       lookupNamespaceURI = resolver.lookupNamespaceURI("nist");
      assertEquals("nistequal","http://www.nist.gov",lookupNamespaceURI);
       
	}
   
	}
   
}




function runTest() {
   XPathNSResolver_lookupNamespaceURI_nist_dmstc();
}
