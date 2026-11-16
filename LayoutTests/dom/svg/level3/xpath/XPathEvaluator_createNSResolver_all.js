
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
return ['XPathEvaluator_createNSResolver_all'];
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
      Iterate over all nodes in the test document, creating
      XPathNSResolvers checking that none return a null object.
    
* @author Bob Clary
* @see http://www.w3.org/TR/2003/CR-DOM-Level-3-XPath-20030331/xpath#XPathEvaluator
* @see http://www.w3.org/TR/2003/CR-DOM-Level-3-XPath-20030331/xpath#XPathEvaluator-createNSResolver
*/
function XPathEvaluator_createNSResolver_all() {
   var success;
    if(checkInitialization(builder, "XPathEvaluator_createNSResolver_all") != null) return;
    var doc;
      var staff;
      var staffchildren;
      var staffchild;
      var staffgrandchildren;
      var staffgrandchild;
      var staffgreatgrandchildren;
      var staffgreatgrandchild;
      var resolver;
      var evaluator;
      
      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      doc = load(docRef, "doc", "staffNS");
      evaluator = createXPathEvaluator(doc);
resolver = evaluator.createNSResolver(doc);
      assertNotNull("documentnotnull",resolver);
staff = doc.documentElement;

      resolver = evaluator.createNSResolver(staff);
      assertNotNull("documentElementnotnull",resolver);
staffchildren = staff.childNodes;

      for(var indexN65644 = 0;indexN65644 < staffchildren.length; indexN65644++) {
      staffchild = staffchildren.item(indexN65644);
      resolver = evaluator.createNSResolver(staffchild);
      assertNotNull("staffchildnotnull",resolver);
staffgrandchildren = staffchild.childNodes;

      for(var indexN65661 = 0;indexN65661 < staffgrandchildren.length; indexN65661++) {
      staffgrandchild = staffgrandchildren.item(indexN65661);
      resolver = evaluator.createNSResolver(staffgrandchild);
      assertNotNull("staffgrandchildnotnull",resolver);
staffgreatgrandchildren = staffgrandchild.childNodes;

      for(var indexN65678 = 0;indexN65678 < staffgreatgrandchildren.length; indexN65678++) {
      staffgreatgrandchild = staffgreatgrandchildren.item(indexN65678);
      resolver = evaluator.createNSResolver(staffgreatgrandchild);
      assertNotNull("staffgreatgrandchildnotnull",resolver);

	}
   
	}
   
	}
   
}




function runTest() {
   XPathEvaluator_createNSResolver_all();
}
