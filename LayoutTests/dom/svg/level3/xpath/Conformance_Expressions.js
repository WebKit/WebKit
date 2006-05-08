
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
return ['Conformance_Expressions'];
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
      1.3 Conformance - Iterate over a list of strings containing 
      valid XPath expressions, calling XPathEvaluator.createExpression 
      for each. If no expections are thrown and each result is non-null, 
      then the test passes.
    
* @author Bob Clary
* @see http://www.w3.org/TR/2003/CR-DOM-Level-3-XPath-20030331/xpath#Conformance
* @see http://www.w3.org/TR/2003/CR-DOM-Level-3-XPath-20030331/xpath#XPathEvaluator
* @see http://www.w3.org/TR/2003/CR-DOM-Level-3-XPath-20030331/xpath#XPathEvaluator-createNSResolver
* @see http://www.w3.org/TR/2003/CR-DOM-Level-3-XPath-20030331/xpath#XPathEvaluator-createExpression
* @see http://www.w3.org/TR/2003/CR-DOM-Level-3-XPath-20030331/xpath#XPathNSResolver
*/
function Conformance_Expressions() {
   var success;
    if(checkInitialization(builder, "Conformance_Expressions") != null) return;
    var doc;
      var resolver;
      var evaluator;
      var expression;
      var expressionList = new Array();

      var xpathexpression;
      
      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      doc = load(docRef, "doc", "staffNS");
      evaluator = createXPathEvaluator(doc);
resolver = evaluator.createNSResolver(doc);
      expressionList[expressionList.length] = "/";
expressionList[expressionList.length] = "child::comment()";
expressionList[expressionList.length] = "child::text()";
expressionList[expressionList.length] = "child::processing-instruction()";
expressionList[expressionList.length] = "child::processing-instruction('name')";
expressionList[expressionList.length] = "child::node()";
expressionList[expressionList.length] = "child::*";
expressionList[expressionList.length] = "child::nist:*";
expressionList[expressionList.length] = "child::employee";
expressionList[expressionList.length] = "descendant::comment()";
expressionList[expressionList.length] = "descendant::text()";
expressionList[expressionList.length] = "descendant::processing-instruction()";
expressionList[expressionList.length] = "descendant::processing-instruction('name')";
expressionList[expressionList.length] = "descendant::node()";
expressionList[expressionList.length] = "descendant::*";
expressionList[expressionList.length] = "descendant::nist:*";
expressionList[expressionList.length] = "descendant::employee";
expressionList[expressionList.length] = "parent::comment()";
expressionList[expressionList.length] = "parent::text()";
expressionList[expressionList.length] = "parent::processing-instruction()";
expressionList[expressionList.length] = "parent::processing-instruction('name')";
expressionList[expressionList.length] = "parent::node()";
expressionList[expressionList.length] = "parent::*";
expressionList[expressionList.length] = "parent::nist:*";
expressionList[expressionList.length] = "parent::employee";
expressionList[expressionList.length] = "ancestor::comment()";
expressionList[expressionList.length] = "ancestor::text()";
expressionList[expressionList.length] = "ancestor::processing-instruction()";
expressionList[expressionList.length] = "ancestor::processing-instruction('name')";
expressionList[expressionList.length] = "ancestor::node()";
expressionList[expressionList.length] = "ancestor::*";
expressionList[expressionList.length] = "ancestor::nist:*";
expressionList[expressionList.length] = "ancestor::employee";
expressionList[expressionList.length] = "following-sibling::comment()";
expressionList[expressionList.length] = "following-sibling::text()";
expressionList[expressionList.length] = "following-sibling::processing-instruction()";
expressionList[expressionList.length] = "following-sibling::processing-instruction('name')";
expressionList[expressionList.length] = "following-sibling::node()";
expressionList[expressionList.length] = "following-sibling::*";
expressionList[expressionList.length] = "following-sibling::nist:*";
expressionList[expressionList.length] = "following-sibling::employee";
expressionList[expressionList.length] = "preceding-sibling::comment()";
expressionList[expressionList.length] = "preceding-sibling::text()";
expressionList[expressionList.length] = "preceding-sibling::processing-instruction()";
expressionList[expressionList.length] = "preceding-sibling::processing-instruction('name')";
expressionList[expressionList.length] = "preceding-sibling::node()";
expressionList[expressionList.length] = "preceding-sibling::*";
expressionList[expressionList.length] = "preceding-sibling::nist:*";
expressionList[expressionList.length] = "preceding-sibling::employee";
expressionList[expressionList.length] = "following::comment()";
expressionList[expressionList.length] = "following::text()";
expressionList[expressionList.length] = "following::processing-instruction()";
expressionList[expressionList.length] = "following::processing-instruction('name')";
expressionList[expressionList.length] = "following::node()";
expressionList[expressionList.length] = "following::*";
expressionList[expressionList.length] = "following::nist:*";
expressionList[expressionList.length] = "following::employee";
expressionList[expressionList.length] = "preceding::comment()";
expressionList[expressionList.length] = "preceding::text()";
expressionList[expressionList.length] = "preceding::processing-instruction()";
expressionList[expressionList.length] = "preceding::processing-instruction('name')";
expressionList[expressionList.length] = "preceding::node()";
expressionList[expressionList.length] = "preceding::*";
expressionList[expressionList.length] = "preceding::nist:*";
expressionList[expressionList.length] = "preceding::employee";
expressionList[expressionList.length] = "attribute::comment()";
expressionList[expressionList.length] = "attribute::text()";
expressionList[expressionList.length] = "attribute::processing-instruction()";
expressionList[expressionList.length] = "attribute::processing-instruction('name')";
expressionList[expressionList.length] = "attribute::node()";
expressionList[expressionList.length] = "attribute::*";
expressionList[expressionList.length] = "attribute::nist:*";
expressionList[expressionList.length] = "attribute::employee";
expressionList[expressionList.length] = "namespace::comment()";
expressionList[expressionList.length] = "namespace::text()";
expressionList[expressionList.length] = "namespace::processing-instruction()";
expressionList[expressionList.length] = "namespace::processing-instruction('name')";
expressionList[expressionList.length] = "namespace::node()";
expressionList[expressionList.length] = "namespace::*";
expressionList[expressionList.length] = "namespace::nist:*";
expressionList[expressionList.length] = "namespace::employee";
expressionList[expressionList.length] = "self::comment()";
expressionList[expressionList.length] = "self::text()";
expressionList[expressionList.length] = "self::processing-instruction()";
expressionList[expressionList.length] = "self::processing-instruction('name')";
expressionList[expressionList.length] = "self::node()";
expressionList[expressionList.length] = "self::*";
expressionList[expressionList.length] = "self::nist:*";
expressionList[expressionList.length] = "self::employee";
expressionList[expressionList.length] = "descendant-or-self::comment()";
expressionList[expressionList.length] = "descendant-or-self::text()";
expressionList[expressionList.length] = "descendant-or-self::processing-instruction()";
expressionList[expressionList.length] = "descendant-or-self::processing-instruction('name')";
expressionList[expressionList.length] = "descendant-or-self::node()";
expressionList[expressionList.length] = "descendant-or-self::*";
expressionList[expressionList.length] = "descendant-or-self::nist:*";
expressionList[expressionList.length] = "descendant-or-self::employee";
expressionList[expressionList.length] = "ancestor-or-self::comment()";
expressionList[expressionList.length] = "ancestor-or-self::text()";
expressionList[expressionList.length] = "ancestor-or-self::processing-instruction()";
expressionList[expressionList.length] = "ancestor-or-self::processing-instruction('name')";
expressionList[expressionList.length] = "ancestor-or-self::node()";
expressionList[expressionList.length] = "ancestor-or-self::*";
expressionList[expressionList.length] = "ancestor-or-self::nist:*";
expressionList[expressionList.length] = "ancestor-or-self::employee";
expressionList[expressionList.length] = "comment()";
expressionList[expressionList.length] = "text()";
expressionList[expressionList.length] = "processing-instruction()";
expressionList[expressionList.length] = "processing-instruction('name')";
expressionList[expressionList.length] = "node()";
expressionList[expressionList.length] = "*";
expressionList[expressionList.length] = "nist:*";
expressionList[expressionList.length] = "employee";
expressionList[expressionList.length] = ".//comment()";
expressionList[expressionList.length] = ".//text()";
expressionList[expressionList.length] = ".//processing-instruction()";
expressionList[expressionList.length] = ".//processing-instruction('name')";
expressionList[expressionList.length] = ".//node()";
expressionList[expressionList.length] = ".//*";
expressionList[expressionList.length] = ".//nist:*";
expressionList[expressionList.length] = ".//employee";
expressionList[expressionList.length] = "../comment()";
expressionList[expressionList.length] = "../text()";
expressionList[expressionList.length] = "../processing-instruction()";
expressionList[expressionList.length] = "../processing-instruction('name')";
expressionList[expressionList.length] = "../node()";
expressionList[expressionList.length] = "../*";
expressionList[expressionList.length] = "../nist:*";
expressionList[expressionList.length] = "../employee";
expressionList[expressionList.length] = "@attributename";
expressionList[expressionList.length] = "./comment()";
expressionList[expressionList.length] = "./text()";
expressionList[expressionList.length] = "./processing-instruction()";
expressionList[expressionList.length] = "./processing-instruction('name')";
expressionList[expressionList.length] = "./node()";
expressionList[expressionList.length] = "./*";
expressionList[expressionList.length] = "./nist:*";
expressionList[expressionList.length] = "./employee";
expressionList[expressionList.length] = "comment() | text() | processing-instruction() | node()";
expressionList[expressionList.length] = "employee[address]";
expressionList[expressionList.length] = "employee/address[@street]";
expressionList[expressionList.length] = "employee[position='Computer Specialist']";
expressionList[expressionList.length] = "employee[position!='Computer Specialist']";
expressionList[expressionList.length] = "employee[gender='Male' or gender='Female']";
expressionList[expressionList.length] = "employee[gender!='Male' and gender!='Female']";
expressionList[expressionList.length] = "employee/address[@street='Yes']";
expressionList[expressionList.length] = "employee/address[@street!='Yes']";
expressionList[expressionList.length] = "employee[position()=1]";
expressionList[expressionList.length] = "employee[1]";
expressionList[expressionList.length] = "employee[position()=last()]";
expressionList[expressionList.length] = "employee[last()]";
expressionList[expressionList.length] = "employee[position()>1 and position<last()]";
expressionList[expressionList.length] = "employee[position()>=1 and position<=last()]";
expressionList[expressionList.length] = "employee[count(.)>0]";
expressionList[expressionList.length] = "employee[position() mod 2=0]";
expressionList[expressionList.length] = "employee[position() mod -2=0]";
expressionList[expressionList.length] = "employee[position() div 2=0]";
expressionList[expressionList.length] = "employee[position() div -2=-1]";
expressionList[expressionList.length] = "employee[position() div 2 * 2=position()]";
expressionList[expressionList.length] = "employee[3 > 2 > 1]";
expressionList[expressionList.length] = "id('CANADA')";
expressionList[expressionList.length] = "*[local-name()='employee']";
expressionList[expressionList.length] = "*[local-name(.)='employee']";
expressionList[expressionList.length] = "*[local-name(employee)='employee']";
expressionList[expressionList.length] = "*[local-name()='employee']";
expressionList[expressionList.length] = "*[namespace-uri()='http://www.nist.gov']";
expressionList[expressionList.length] = "*[name()='nist:employee']";
expressionList[expressionList.length] = "*[string()]";
expressionList[expressionList.length] = "*[string(10 div foo)='NaN']";
expressionList[expressionList.length] = "*[concat('a', 'b', 'c')]";
expressionList[expressionList.length] = "*[starts-with('employee', 'emp')]";
expressionList[expressionList.length] = "*[contains('employee', 'emp')]";
expressionList[expressionList.length] = "*[substring-before('employeeId', 'Id')]";
expressionList[expressionList.length] = "*[substring-after('employeeId', 'employee')]";
expressionList[expressionList.length] = "*[substring('employeeId', 4)]";
expressionList[expressionList.length] = "*[substring('employeeId', 4, 5)]";
expressionList[expressionList.length] = "*[string-length()=2]";
expressionList[expressionList.length] = "*[string-length(.)=string-length(normalize-space(.))]";
expressionList[expressionList.length] = "*[translate('bar', 'abc', 'ABC')='BAr']";
expressionList[expressionList.length] = "*[boolean(.)]";
expressionList[expressionList.length] = "*[not(boolean(.))]";
expressionList[expressionList.length] = "*[true()]";
expressionList[expressionList.length] = "*[false()]";
expressionList[expressionList.length] = "*[lang('en')]";
expressionList[expressionList.length] = "*[number()]";
expressionList[expressionList.length] = "*[number('4')]";
expressionList[expressionList.length] = "*[floor(.)]>0";
expressionList[expressionList.length] = "*[ceiling(.)]<1";
expressionList[expressionList.length] = "*[round(number(.))=0]<1";
for(var indexN66388 = 0;indexN66388 < expressionList.length; indexN66388++) {
      expression = expressionList[indexN66388];
      xpathexpression = evaluator.createExpression(expression,resolver);
      
	}
   
}




function runTest() {
   Conformance_Expressions();
}
