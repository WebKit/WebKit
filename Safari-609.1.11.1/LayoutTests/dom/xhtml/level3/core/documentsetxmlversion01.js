
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
      return "http://www.w3.org/2001/DOM-Test-Suite/level3/core/documentsetxmlversion01";
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
	Set the value of the version attribute of the XML declaration of this document to 
	various invalid characters and  verify if a NOT_SUPPORTED_ERR is thrown.

* @author IBM
* @author Neil Delima
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#Document3-version
*/
function documentsetxmlversion01() {
   var success;
    if(checkInitialization(builder, "documentsetxmlversion01") != null) return;
    var doc;
      var versionValue;
      illegalVersion = new Array();
      illegalVersion[0] = "{";
      illegalVersion[1] = "}";
      illegalVersion[2] = "~";
      illegalVersion[3] = "'";
      illegalVersion[4] = "!";
      illegalVersion[5] = "@";
      illegalVersion[6] = "#";
      illegalVersion[7] = "$";
      illegalVersion[8] = "%";
      illegalVersion[9] = "^";
      illegalVersion[10] = "&";
      illegalVersion[11] = "*";
      illegalVersion[12] = "(";
      illegalVersion[13] = ")";
      illegalVersion[14] = "+";
      illegalVersion[15] = "=";
      illegalVersion[16] = "[";
      illegalVersion[17] = "]";
      illegalVersion[18] = "\\";
      illegalVersion[19] = "/";
      illegalVersion[20] = ";";
      illegalVersion[21] = "`";
      illegalVersion[22] = "<";
      illegalVersion[23] = ">";
      illegalVersion[24] = ",";
      illegalVersion[25] = "a ";
      illegalVersion[26] = "\"";
      illegalVersion[27] = "---";

      
      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      doc = load(docRef, "doc", "hc_staff");
      for(var indexN10087 = 0;indexN10087 < illegalVersion.length; indexN10087++) {
      versionValue = illegalVersion[indexN10087];
      
	{
		success = false;
		try {
            doc.xmlVersion = versionValue;

        }
		catch(ex) {
      success = (typeof(ex.code) != 'undefined' && ex.code == 9);
		}
		assertTrue("NOT_SUPPORTED_ERR_documentsetversion01",success);
	}

	}
   
}




function runTest() {
   documentsetxmlversion01();
}
