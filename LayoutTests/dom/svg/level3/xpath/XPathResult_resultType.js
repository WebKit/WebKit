
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
return ['XPathResult_resultType'];
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
      Create an XPathResult for the expression /staff/employee
      for each type of XPathResultType, checking that the resultType
      of the XPathResult matches the requested type.
    
* @author Bob Clary
* @see http://www.w3.org/TR/2003/CR-DOM-Level-3-XPath-20030331/xpath#XPathResultType
* @see http://www.w3.org/TR/2003/CR-DOM-Level-3-XPath-20030331/xpath#XPathResult
* @see http://www.w3.org/TR/2003/CR-DOM-Level-3-XPath-20030331/xpath#XPathEvaluator-createNSResolver
* @see http://www.w3.org/TR/2003/CR-DOM-Level-3-XPath-20030331/xpath#XPathEvaluator-evaluate
* @see http://www.w3.org/TR/2003/CR-DOM-Level-3-XPath-20030331/xpath#XPathResult-resultType
* @see http://www.w3.org/TR/2003/CR-DOM-Level-3-XPath-20030331/xpath#XPathException
*/
function XPathResult_resultType() {
   var success;
    if(checkInitialization(builder, "XPathResult_resultType") != null) return;
    var doc;
      var resolver;
      var evaluator;
      var expression = "/staff/employee";
      var contextNode;
      var inresult = null;

      var outresult = null;

      var inNodeType;
      var outNodeType;
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
      var isTypeEqual;
      nodeTypeList = new Array();
      nodeTypeList[0] = 0;
      nodeTypeList[1] = 1;
      nodeTypeList[2] = 2;
      nodeTypeList[3] = 3;
      nodeTypeList[4] = 4;
      nodeTypeList[5] = 5;
      nodeTypeList[6] = 6;
      nodeTypeList[7] = 7;
      nodeTypeList[8] = 8;
      nodeTypeList[9] = 9;

      
      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      doc = load(docRef, "doc", "staff");
      evaluator = createXPathEvaluator(doc);
resolver = evaluator.createNSResolver(doc);
      contextNode =  doc;
for(var indexN65737 = 0;indexN65737 < nodeTypeList.length; indexN65737++) {
      inNodeType = nodeTypeList[indexN65737];
      outresult = evaluator.evaluate(expression,contextNode,resolver,inNodeType,inresult);
      outNodeType = outresult.resultType;

      
	if(
	(inNodeType == ANY_TYPE)
	) {
	assertEquals("ANY_TYPE_resulttype",UNORDERED_NODE_ITERATOR_TYPE,outNodeType);
       
	}
	
	if(
	(inNodeType == NUMBER_TYPE)
	) {
	assertEquals("NUMBER_TYPE_resulttype",NUMBER_TYPE,outNodeType);
       
	}
	
	if(
	(inNodeType == STRING_TYPE)
	) {
	assertEquals("STRING_TYPE_resulttype",STRING_TYPE,outNodeType);
       
	}
	
	if(
	(inNodeType == BOOLEAN_TYPE)
	) {
	assertEquals("BOOLEAN_TYPE_resulttype",BOOLEAN_TYPE,outNodeType);
       
	}
	
	if(
	(inNodeType == UNORDERED_NODE_ITERATOR_TYPE)
	) {
	assertEquals("UNORDERED_NODE_ITERATOR_TYPE_resulttype",UNORDERED_NODE_ITERATOR_TYPE,outNodeType);
       
	}
	
	if(
	(inNodeType == ORDERED_NODE_ITERATOR_TYPE)
	) {
	assertEquals("ORDERED_NODE_ITERATOR_TYPE_resulttype",ORDERED_NODE_ITERATOR_TYPE,outNodeType);
       
	}
	
	if(
	(inNodeType == UNORDERED_NODE_SNAPSHOT_TYPE)
	) {
	assertEquals("UNORDERED_NODE_SNAPSHOT_TYPE_resulttype",UNORDERED_NODE_SNAPSHOT_TYPE,outNodeType);
       
	}
	
	if(
	(inNodeType == ORDERED_NODE_SNAPSHOT_TYPE)
	) {
	assertEquals("ORDERED_NODE_SNAPSHOT_TYPE_resulttype",ORDERED_NODE_SNAPSHOT_TYPE,outNodeType);
       
	}
	
	if(
	(inNodeType == ANY_UNORDERED_NODE_TYPE)
	) {
	assertEquals("ANY_UNORDERED_NODE_TYPE_resulttype",ANY_UNORDERED_NODE_TYPE,outNodeType);
       
	}
	
	if(
	(inNodeType == FIRST_ORDERED_NODE_TYPE)
	) {
	assertEquals("FIRST_ORDERED_NODE_TYPE_resulttype",FIRST_ORDERED_NODE_TYPE,outNodeType);
       
	}
	
	}
   
}




function runTest() {
   XPathResult_resultType();
}
