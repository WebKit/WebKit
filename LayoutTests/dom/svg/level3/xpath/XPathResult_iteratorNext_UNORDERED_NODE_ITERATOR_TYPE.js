
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
return ['XPathResult_iteratorNext_UNORDERED_NODE_ITERATOR_TYPE'];
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
      docsLoaded += preload(docRef, "doc", "staff");
        
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
    Create an XPathResult UNORDERED_NODE_ITERATOR_TYPE XPathResultType for 
    expression /staff/employee/employeeId/text() checking that:
    XPathResult.iteratorNext contains the correct number of nodes.
    
* @author Bob Clary
* @see http://www.w3.org/TR/2003/CR-DOM-Level-3-XPath-20030331/xpath#XPathResult-iteratorNext
* @see http://www.w3.org/TR/2003/CR-DOM-Level-3-XPath-20030331/xpath#XPathResult
* @see http://www.w3.org/TR/2003/CR-DOM-Level-3-XPath-20030331/xpath#XPathResultType
* @see http://www.w3.org/TR/2003/CR-DOM-Level-3-XPath-20030331/xpath#XPathEvaluator-createNSResolver
*/
function XPathResult_iteratorNext_UNORDERED_NODE_ITERATOR_TYPE() {
   var success;
    if(checkInitialization(builder, "XPathResult_iteratorNext_UNORDERED_NODE_ITERATOR_TYPE") != null) return;
    var ANY_TYPE = 0;
      var NUMBER_TYPE = 1;
      var STRING_TYPE = 2;
      var BOOLEAN_TYPE = 3;
      var UNORDERED_NODE_ITERATOR_TYPE = 4;
      var ORDERED_NODE_ITERATOR_TYPE = 5;
      var UNORDERED_NODE_SNAPSHOT_TYPE = 6;
      var ORDERED_NODE_SNAPSHOT_TYPE = 7;
      var ANY_UNORDERED_NODE_TYPE = 8;
      var FIRST_ORDERED_NODE_TYPE = 9;
      var doc;
      var resolver;
      var evaluator;
      var contextNode;
      var inresult = null;

      var outresult = null;

      var expression = "/staff/employee/employeeId/text()";
      var xpathType = UNORDERED_NODE_ITERATOR_TYPE;
      var outNode;
      var nodeType;
      var parent;
      var owner;
      var ownerType;
      var index;
      
      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      doc = load(docRef, "doc", "staff");
      evaluator = createXPathEvaluator(doc);
resolver = evaluator.createNSResolver(doc);
      contextNode =  doc;
outresult = evaluator.evaluate(expression,contextNode,resolver,xpathType,inresult);
      index = 0;
outNode = outresult.iterateNext();
      
    while(
	
	(outNode != null)

	) {
	index += 1;
outNode = outresult.iterateNext();
      
	}
assertEquals("count",4,index);
       
}




function runTest() {
   XPathResult_iteratorNext_UNORDERED_NODE_ITERATOR_TYPE();
}
