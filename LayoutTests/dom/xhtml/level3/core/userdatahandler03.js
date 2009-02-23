
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
      return "http://www.w3.org/2001/DOM-Test-Suite/level3/core/userdatahandler03";
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

//UserDataMonitor's require a document level variable named userDataMonitor
var userDataMonitor;
	 
/**
* 
Call setUserData on a node providing a UserDataHandler and import the node.

* @author Curt Arnold
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#ID-handleUserDataEvent
*/
function userdatahandler03() {
   var success;
    if(checkInitialization(builder, "userdatahandler03") != null) return;
    var doc;
      var node;
      var pList;
      userDataMonitor = new UserDataMonitor();
      
      var oldUserData;
      var elementNS;
      var newNode;
      var notifications = new Array();

      var notification;
      var operation;
      var key;
      var data;
      var src;
      var dst;
      var greetingCount = 0;
      var salutationCount = 0;
      var hello = "Hello";
      var mister = "Mr.";
      var newDoc;
      var rootName;
      var rootNS;
      var domImpl;
      var docType = null;

      var docElem;
      
      var docRef = null;
      if (typeof(this.doc) != 'undefined') {
        docRef = this.doc;
      }
      doc = load(docRef, "doc", "barfoo");
      domImpl = doc.implementation;
docElem = doc.documentElement;

      rootNS = docElem.namespaceURI;

      rootName = docElem.tagName;

      newDoc = domImpl.createDocument(rootNS,rootName,docType);
      pList = doc.getElementsByTagName("p");
      node = pList.item(0);
      if (null == userDataMonitor) {
         node.setUserData("greeting", hello, null);
      } else {
          node.setUserData("greeting", hello, userDataMonitor.handle);
      }
       if (null == userDataMonitor) {
         node.setUserData("salutation", mister, null);
      } else {
          node.setUserData("salutation", mister, userDataMonitor.handle);
      }
       elementNS = node.namespaceURI;

      newNode = doc.importNode(node,true);
      notifications = userDataMonitor.allNotifications;
assertSize("twoNotifications",2,notifications);
for(var indexN100CE = 0;indexN100CE < notifications.length; indexN100CE++) {
      notification = notifications[indexN100CE];
      operation = notification.operation;
assertEquals("operationIsImport",2,operation);
       key = notification.key;
data = notification.data;

	if(
	("greeting" == key)
	) {
	assertEquals("greetingDataHello",hello,data);
       greetingCount += 1;

	}
	
		else {
			assertEquals("saluationKey","salutation",key);
       assertEquals("salutationDataMr",mister,data);
       salutationCount += 1;

		}
	src = notification.src;
assertSame("srcIsNode",node,src);
dst = notification.dst;
assertSame("dstIsNewNode",newNode,dst);

	}
   assertEquals("greetingCountIs1",1,greetingCount);
       assertEquals("salutationCountIs1",1,salutationCount);
       
}




function runTest() {
   userdatahandler03();
}
