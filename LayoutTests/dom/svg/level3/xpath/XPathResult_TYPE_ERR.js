
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
return ['XPathResult_TYPE_ERR'];
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
      for each type of XPathResultType, checking that TYPE_ERR
      is thrown when inappropriate properties and methods are accessed.
    
* @author Bob Clary
* @see http://www.w3.org/TR/2003/CR-DOM-Level-3-XPath-20030331/xpath#TYPE_ERR
* @see http://www.w3.org/TR/2003/CR-DOM-Level-3-XPath-20030331/xpath#XPathException
* @see http://www.w3.org/TR/2003/CR-DOM-Level-3-XPath-20030331/xpath#XPathResult
* @see http://www.w3.org/TR/2003/CR-DOM-Level-3-XPath-20030331/xpath#XPathResultType
* @see http://www.w3.org/TR/2003/CR-DOM-Level-3-XPath-20030331/xpath#XPathEvaluator-createNSResolver
* @see http://www.w3.org/TR/2003/CR-DOM-Level-3-XPath-20030331/xpath#XPathResult-resultType
* @see http://www.w3.org/TR/2003/CR-DOM-Level-3-XPath-20030331/xpath#XPathResult-booleanValue
* @see http://www.w3.org/TR/2003/CR-DOM-Level-3-XPath-20030331/xpath#XPathResult-numberValue
* @see http://www.w3.org/TR/2003/CR-DOM-Level-3-XPath-20030331/xpath#XPathResult-singleNodeValue
* @see http://www.w3.org/TR/2003/CR-DOM-Level-3-XPath-20030331/xpath#XPathResult-snapshot-length
* @see http://www.w3.org/TR/2003/CR-DOM-Level-3-XPath-20030331/xpath#XPathResult-stringValue
* @see http://www.w3.org/TR/2003/CR-DOM-Level-3-XPath-20030331/xpath#XPathResult-iterateNext
* @see http://www.w3.org/TR/2003/CR-DOM-Level-3-XPath-20030331/xpath#XPathResult-snapshotItem
*/
function XPathResult_TYPE_ERR() {
   var success;
    if(checkInitialization(builder, "XPathResult_TYPE_ERR") != null) return;
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
      var booleanValue;
      var shortValue;
      var intValue;
      var doubleValue;
      var nodeValue;
      var stringValue;
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
for(var indexN65778 = 0;indexN65778 < nodeTypeList.length; indexN65778++) {
      inNodeType = nodeTypeList[indexN65778];
      outresult = evaluator.evaluate(expression,contextNode,resolver,inNodeType,inresult);
      outNodeType = outresult.resultType;

      
	if(
	(outNodeType == NUMBER_TYPE)
	) {
	
	{
		success = false;
		try {
            booleanValue = outresult.booleanValue;

        }
		catch(ex) {            
      success = (typeof(ex.code) != 'undefined' && ex.code == 52);
		}
		assertTrue("number_booleanValue_TYPE_ERR",success);
	}

	{
		success = false;
		try {
            nodeValue = outresult.singleNodeValue;

        }
		catch(ex) {            
      success = (typeof(ex.code) != 'undefined' && ex.code == 52);
		}
		assertTrue("number_singleNodeValue_TYPE_ERR",success);
	}

	{
		success = false;
		try {
            intValue = outresult.snapshotLength;

        }
		catch(ex) {            
      success = (typeof(ex.code) != 'undefined' && ex.code == 52);
		}
		assertTrue("number_snapshotLength_TYPE_ERR",success);
	}

	{
		success = false;
		try {
            stringValue = outresult.stringValue;

        }
		catch(ex) {            
      success = (typeof(ex.code) != 'undefined' && ex.code == 52);
		}
		assertTrue("number_stringValue_TYPE_ERR",success);
	}

	{
		success = false;
		try {
            nodeValue = outresult.iterateNext();
        }
		catch(ex) {            
      success = (typeof(ex.code) != 'undefined' && ex.code == 52);
		}
		assertTrue("number_iterateNext_TYPE_ERR",success);
	}

	{
		success = false;
		try {
            nodeValue = outresult.snapshotItem(0);
        }
		catch(ex) {            
      success = (typeof(ex.code) != 'undefined' && ex.code == 52);
		}
		assertTrue("number_snapshotItem_TYPE_ERR",success);
	}

	}
	
	if(
	(outNodeType == STRING_TYPE)
	) {
	
	{
		success = false;
		try {
            booleanValue = outresult.booleanValue;

        }
		catch(ex) {            
      success = (typeof(ex.code) != 'undefined' && ex.code == 52);
		}
		assertTrue("string_booleanValue_TYPE_ERR",success);
	}

	{
		success = false;
		try {
            doubleValue = outresult.numberValue;

        }
		catch(ex) {            
      success = (typeof(ex.code) != 'undefined' && ex.code == 52);
		}
		assertTrue("string_numberValue_TYPE_ERR",success);
	}

	{
		success = false;
		try {
            nodeValue = outresult.singleNodeValue;

        }
		catch(ex) {            
      success = (typeof(ex.code) != 'undefined' && ex.code == 52);
		}
		assertTrue("string_singleNodeValue_TYPE_ERR",success);
	}

	{
		success = false;
		try {
            intValue = outresult.snapshotLength;

        }
		catch(ex) {            
      success = (typeof(ex.code) != 'undefined' && ex.code == 52);
		}
		assertTrue("string_snapshotLength_TYPE_ERR",success);
	}

	{
		success = false;
		try {
            nodeValue = outresult.iterateNext();
        }
		catch(ex) {            
      success = (typeof(ex.code) != 'undefined' && ex.code == 52);
		}
		assertTrue("string_iterateNext_TYPE_ERR",success);
	}

	{
		success = false;
		try {
            nodeValue = outresult.snapshotItem(0);
        }
		catch(ex) {            
      success = (typeof(ex.code) != 'undefined' && ex.code == 52);
		}
		assertTrue("string_snapshotItem_TYPE_ERR",success);
	}

	}
	
	if(
	(outNodeType == BOOLEAN_TYPE)
	) {
	
	{
		success = false;
		try {
            doubleValue = outresult.numberValue;

        }
		catch(ex) {            
      success = (typeof(ex.code) != 'undefined' && ex.code == 52);
		}
		assertTrue("boolean_numberValue_TYPE_ERR",success);
	}

	{
		success = false;
		try {
            nodeValue = outresult.singleNodeValue;

        }
		catch(ex) {            
      success = (typeof(ex.code) != 'undefined' && ex.code == 52);
		}
		assertTrue("boolean_singleNodeValue_TYPE_ERR",success);
	}

	{
		success = false;
		try {
            intValue = outresult.snapshotLength;

        }
		catch(ex) {            
      success = (typeof(ex.code) != 'undefined' && ex.code == 52);
		}
		assertTrue("boolean_snapshotLength_TYPE_ERR",success);
	}

	{
		success = false;
		try {
            stringValue = outresult.stringValue;

        }
		catch(ex) {            
      success = (typeof(ex.code) != 'undefined' && ex.code == 52);
		}
		assertTrue("boolean_stringValue_TYPE_ERR",success);
	}

	{
		success = false;
		try {
            nodeValue = outresult.iterateNext();
        }
		catch(ex) {            
      success = (typeof(ex.code) != 'undefined' && ex.code == 52);
		}
		assertTrue("boolean_iterateNext_TYPE_ERR",success);
	}

	{
		success = false;
		try {
            nodeValue = outresult.snapshotItem(0);
        }
		catch(ex) {            
      success = (typeof(ex.code) != 'undefined' && ex.code == 52);
		}
		assertTrue("boolean_snapshotItem_TYPE_ERR",success);
	}

	}
	
	if(
	(outNodeType == UNORDERED_NODE_ITERATOR_TYPE)
	) {
	
	{
		success = false;
		try {
            booleanValue = outresult.booleanValue;

        }
		catch(ex) {            
      success = (typeof(ex.code) != 'undefined' && ex.code == 52);
		}
		assertTrue("unordered_node_iterator_booleanValue_TYPE_ERR",success);
	}

	{
		success = false;
		try {
            doubleValue = outresult.numberValue;

        }
		catch(ex) {            
      success = (typeof(ex.code) != 'undefined' && ex.code == 52);
		}
		assertTrue("unordered_node_iterator_numberValue_TYPE_ERR",success);
	}

	{
		success = false;
		try {
            nodeValue = outresult.singleNodeValue;

        }
		catch(ex) {            
      success = (typeof(ex.code) != 'undefined' && ex.code == 52);
		}
		assertTrue("unordered_node_iterator_singleNodeValue_TYPE_ERR",success);
	}

	{
		success = false;
		try {
            intValue = outresult.snapshotLength;

        }
		catch(ex) {            
      success = (typeof(ex.code) != 'undefined' && ex.code == 52);
		}
		assertTrue("unordered_node_iterator_snapshotLength_TYPE_ERR",success);
	}

	{
		success = false;
		try {
            stringValue = outresult.stringValue;

        }
		catch(ex) {            
      success = (typeof(ex.code) != 'undefined' && ex.code == 52);
		}
		assertTrue("unordered_node_iterator_stringValue_TYPE_ERR",success);
	}

	{
		success = false;
		try {
            nodeValue = outresult.snapshotItem(0);
        }
		catch(ex) {            
      success = (typeof(ex.code) != 'undefined' && ex.code == 52);
		}
		assertTrue("unordered_node_iterator_snapshotItem_TYPE_ERR",success);
	}

	}
	
	if(
	(outNodeType == ORDERED_NODE_ITERATOR_TYPE)
	) {
	
	{
		success = false;
		try {
            booleanValue = outresult.booleanValue;

        }
		catch(ex) {            
      success = (typeof(ex.code) != 'undefined' && ex.code == 52);
		}
		assertTrue("ordered_node_iterator_booleanValue_TYPE_ERR",success);
	}

	{
		success = false;
		try {
            doubleValue = outresult.numberValue;

        }
		catch(ex) {            
      success = (typeof(ex.code) != 'undefined' && ex.code == 52);
		}
		assertTrue("ordered_node_iterator_numberValue_TYPE_ERR",success);
	}

	{
		success = false;
		try {
            nodeValue = outresult.singleNodeValue;

        }
		catch(ex) {            
      success = (typeof(ex.code) != 'undefined' && ex.code == 52);
		}
		assertTrue("ordered_node_iterator_singleNodeValue_TYPE_ERR",success);
	}

	{
		success = false;
		try {
            intValue = outresult.snapshotLength;

        }
		catch(ex) {            
      success = (typeof(ex.code) != 'undefined' && ex.code == 52);
		}
		assertTrue("ordered_node_iterator_snapshotLength_TYPE_ERR",success);
	}

	{
		success = false;
		try {
            stringValue = outresult.stringValue;

        }
		catch(ex) {            
      success = (typeof(ex.code) != 'undefined' && ex.code == 52);
		}
		assertTrue("ordered_node_iterator_stringValue_TYPE_ERR",success);
	}

	{
		success = false;
		try {
            nodeValue = outresult.snapshotItem(0);
        }
		catch(ex) {            
      success = (typeof(ex.code) != 'undefined' && ex.code == 52);
		}
		assertTrue("ordered_node_iterator_snapshotItem_TYPE_ERR",success);
	}

	}
	
	if(
	(outNodeType == UNORDERED_NODE_SNAPSHOT_TYPE)
	) {
	
	{
		success = false;
		try {
            booleanValue = outresult.booleanValue;

        }
		catch(ex) {            
      success = (typeof(ex.code) != 'undefined' && ex.code == 52);
		}
		assertTrue("unordered_node_snapshot_booleanValue_TYPE_ERR",success);
	}

	{
		success = false;
		try {
            doubleValue = outresult.numberValue;

        }
		catch(ex) {            
      success = (typeof(ex.code) != 'undefined' && ex.code == 52);
		}
		assertTrue("unordered_node_snapshot_numberValue_TYPE_ERR",success);
	}

	{
		success = false;
		try {
            nodeValue = outresult.singleNodeValue;

        }
		catch(ex) {            
      success = (typeof(ex.code) != 'undefined' && ex.code == 52);
		}
		assertTrue("unordered_node_snapshot_singleNodeValue_TYPE_ERR",success);
	}

	{
		success = false;
		try {
            stringValue = outresult.stringValue;

        }
		catch(ex) {            
      success = (typeof(ex.code) != 'undefined' && ex.code == 52);
		}
		assertTrue("unordered_node_snapshot_stringValue_TYPE_ERR",success);
	}

	{
		success = false;
		try {
            nodeValue = outresult.iterateNext();
        }
		catch(ex) {            
      success = (typeof(ex.code) != 'undefined' && ex.code == 52);
		}
		assertTrue("unordered_node_snapshot_iterateNext_TYPE_ERR",success);
	}

	}
	
	if(
	(outNodeType == ORDERED_NODE_SNAPSHOT_TYPE)
	) {
	
	{
		success = false;
		try {
            booleanValue = outresult.booleanValue;

        }
		catch(ex) {            
      success = (typeof(ex.code) != 'undefined' && ex.code == 52);
		}
		assertTrue("ordered_node_snapshot_booleanValue_TYPE_ERR",success);
	}

	{
		success = false;
		try {
            doubleValue = outresult.numberValue;

        }
		catch(ex) {            
      success = (typeof(ex.code) != 'undefined' && ex.code == 52);
		}
		assertTrue("ordered_node_snapshot_numberValue_TYPE_ERR",success);
	}

	{
		success = false;
		try {
            nodeValue = outresult.singleNodeValue;

        }
		catch(ex) {            
      success = (typeof(ex.code) != 'undefined' && ex.code == 52);
		}
		assertTrue("ordered_node_snapshot_singleNodeValue_TYPE_ERR",success);
	}

	{
		success = false;
		try {
            stringValue = outresult.stringValue;

        }
		catch(ex) {            
      success = (typeof(ex.code) != 'undefined' && ex.code == 52);
		}
		assertTrue("ordered_node_snapshot_stringValue_TYPE_ERR",success);
	}

	{
		success = false;
		try {
            nodeValue = outresult.iterateNext();
        }
		catch(ex) {            
      success = (typeof(ex.code) != 'undefined' && ex.code == 52);
		}
		assertTrue("ordered_node_snapshot_iterateNext_TYPE_ERR",success);
	}

	}
	
	if(
	(outNodeType == ANY_UNORDERED_NODE_TYPE)
	) {
	
	{
		success = false;
		try {
            booleanValue = outresult.booleanValue;

        }
		catch(ex) {            
      success = (typeof(ex.code) != 'undefined' && ex.code == 52);
		}
		assertTrue("any_unordered_node_booleanValue_TYPE_ERR",success);
	}

	{
		success = false;
		try {
            doubleValue = outresult.numberValue;

        }
		catch(ex) {            
      success = (typeof(ex.code) != 'undefined' && ex.code == 52);
		}
		assertTrue("any_unordered_node_numberValue_TYPE_ERR",success);
	}

	{
		success = false;
		try {
            intValue = outresult.snapshotLength;

        }
		catch(ex) {            
      success = (typeof(ex.code) != 'undefined' && ex.code == 52);
		}
		assertTrue("any_unordered_node_snapshotLength_TYPE_ERR",success);
	}

	{
		success = false;
		try {
            stringValue = outresult.stringValue;

        }
		catch(ex) {            
      success = (typeof(ex.code) != 'undefined' && ex.code == 52);
		}
		assertTrue("any_unordered_node_stringValue_TYPE_ERR",success);
	}

	{
		success = false;
		try {
            nodeValue = outresult.iterateNext();
        }
		catch(ex) {            
      success = (typeof(ex.code) != 'undefined' && ex.code == 52);
		}
		assertTrue("any_unordered_node_iterateNext_TYPE_ERR",success);
	}

	{
		success = false;
		try {
            nodeValue = outresult.snapshotItem(0);
        }
		catch(ex) {            
      success = (typeof(ex.code) != 'undefined' && ex.code == 52);
		}
		assertTrue("any_unordered_node_snapshotItem_TYPE_ERR",success);
	}

	}
	
	if(
	(outNodeType == FIRST_ORDERED_NODE_TYPE)
	) {
	
	{
		success = false;
		try {
            booleanValue = outresult.booleanValue;

        }
		catch(ex) {            
      success = (typeof(ex.code) != 'undefined' && ex.code == 52);
		}
		assertTrue("first_ordered_node_booleanValue_TYPE_ERR",success);
	}

	{
		success = false;
		try {
            doubleValue = outresult.numberValue;

        }
		catch(ex) {            
      success = (typeof(ex.code) != 'undefined' && ex.code == 52);
		}
		assertTrue("first_ordered_node_numberValue_TYPE_ERR",success);
	}

	{
		success = false;
		try {
            intValue = outresult.snapshotLength;

        }
		catch(ex) {            
      success = (typeof(ex.code) != 'undefined' && ex.code == 52);
		}
		assertTrue("first_ordered_node_snapshotLength_TYPE_ERR",success);
	}

	{
		success = false;
		try {
            stringValue = outresult.stringValue;

        }
		catch(ex) {            
      success = (typeof(ex.code) != 'undefined' && ex.code == 52);
		}
		assertTrue("first_ordered_node_stringValue_TYPE_ERR",success);
	}

	{
		success = false;
		try {
            nodeValue = outresult.iterateNext();
        }
		catch(ex) {            
      success = (typeof(ex.code) != 'undefined' && ex.code == 52);
		}
		assertTrue("first_ordered_node_iterateNext_TYPE_ERR",success);
	}

	{
		success = false;
		try {
            nodeValue = outresult.snapshotItem(0);
        }
		catch(ex) {            
      success = (typeof(ex.code) != 'undefined' && ex.code == 52);
		}
		assertTrue("first_ordered_node_snapshotItem_TYPE_ERR",success);
	}

	}
	
	}
   
}




function runTest() {
   XPathResult_TYPE_ERR();
}
