//
// This file contains the installation specific values for QuickSearch.
// See quicksearch.js for more details.
//

// the global bugzilla url

var bugzilla = "";
//var bugzilla = "http://bugzilla.mozilla.org/";

// Status and Resolution
// ---------------------

var statuses_open     = new Array("UNCONFIRMED","NEW","ASSIGNED","REOPENED");
var statuses_resolved = new Array("RESOLVED","VERIFIED","CLOSED");
var resolutions       = new Array("FIXED","INVALID","WONTFIX","LATER",
                                  "REMIND","DUPLICATE","WORKSFORME","MOVED");

// Keywords
// --------
//
// Enumerate all your keywords here. This is necessary to avoid 
// "foo is not a legal keyword" errors. This makes it possible
// to include the keywords field in the search by default.

var keywords = new Array(
// "foo", "bar", "baz"
);

// Platforms
// ---------
//
// A list of words <w> (substrings of platform values) 
// that will automatically be translated to "platform:<w>" 
// E.g. if "mac" is defined as a platform, then searching 
// for it will find all bugs with platform="Macintosh", 
// but no other bugs with e.g. "mac" in the summary.

var platforms = new Array(
"pc","sun","macintosh","mac" //shortcut added
//,"dec","hp","sgi" 
//,"all"   //this is a legal value for OpSys, too :(
//,"other" 
);

// Severities
// ----------
//
// A list of words <w> (substrings of severity values)
// that will automatically be translated to "severity:<w>"
// E.g with this default set of severities, searching for
// "blo,cri,maj" will find all severe bugs.

var severities = new Array("blo","cri","maj","nor","min","tri","enh");

// Products and Components
// -----------------------
//
// It is not necessary to list all products and components here.
// Instead, you can define a "blacklist" for some commonly used 
// words or word fragments that occur in a product or component name
// but should _not_ trigger product/component search.

var product_exceptions = new Array(
"row"   // [Browser]
        //   ^^^
,"new"  // [MailNews]
        //      ^^^
);

var component_exceptions = new Array(
"hang"  // [mozilla.org] Bugzilla: Component/Keyword Changes
        //                                            ^^^^
);

