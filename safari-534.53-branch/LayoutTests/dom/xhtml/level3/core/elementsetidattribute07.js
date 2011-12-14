
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
      return "http://www.w3.org/2001/DOM-Test-Suite/level3/core/elementsetidattribute07";
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
      docsLoaded += preload(docRef, "doc", "hc_staff");
        
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
	First use setAttribute to create two new attribute of the second and third strong element with different values.
	Invoke setIdAttribute on the new  attributes. Verify by calling isID on the new attributes and getElementById 
	with two different values on document.	

* @author IBM
* @author Jenny Hsu
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#ID-ElSetIdAttr
*/
function elementsetidattribute07() {
   var success;
    if(checkInitialization(builder, "elementsetidattribute07") != null) return;
    var doc;
      var elemList;
      var nameElem1;
      var nameElem2;
      var attributesMap;
      var attr;
      var id = false;
      var elem;
      var elemName;
      
      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      doc = load(docRef, "doc", "hc_staff");
      elemList = doc.getElementsByTagName("strong");
      nameElem1 = elemList.item(2);
      nameElem2 = elemList.item(3);
      nameElem1.setAttribute("hasMiddleName","Antoine");
      nameElem1.setIdAttribute("hasMiddleName",true);
      nameElem2.setAttribute("hasMiddleName","Neeya");
      nameElem2.setIdAttribute("hasMiddleName",true);
      attributesMap = nameElem1.attributes;

      attr = attributesMap.getNamedItem("hasMiddleName");
      id = attr.isId;

      assertTrue("elementsetidattributeIsId1True07",id);
attributesMap = nameElem2.attributes;

      attr = attributesMap.getNamedItem("hasMiddleName");
      id = attr.isId;

      assertTrue("elementsetidattributeIsId2True07",id);
elem = doc.getElementById("Antoine");
      elemName = elem.tagName;

      assertEquals("elementsetidattribute1GetElementById07","strong",elemName);
       elem = doc.getElementById("Neeya");
      elemName = elem.tagName;

      assertEquals("elementsetidattribute2GetElementById07","strong",elemName);
       
}




function runTest() {
   elementsetidattribute07();
}
