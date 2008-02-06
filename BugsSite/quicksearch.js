//
// This is the main JS file for QuickSearch.
//
// Derived from:
//
//   * C. Begle's SimpleSearch tool:
//     http://www.mozilla.org/quality/help/simplesearch.html
//     http://www.mozilla.org/quality/help/bugreport.js 
//
//   * Jesse Ruderman's bugzilla search page:
//     http://www.cs.hmc.edu/~jruderma/s/bugz.html
//
// Created by
//     Andreas Franke <afranke@mathweb.org>
//
// Contributors:
//     Stephen Lee <slee@uk.bnsmc.com>


// Use no_result variable to avoid problems with "undefined" on some browsers

var no_result="---";

// do_unshift(l, s) is equivalent to l.unshift(s), but some browsers do not
// support the built-in function.

function do_unshift(l, s) {
  l.length = l.length + 1;
  for (var i=l.length-1; i>0; i--) {
    l[i] = l[i-1];
  }
  l[0] = s;
  return l.length;
}

// do_shift(l) is equivalent to l.shift(s), but some browsers do not
// support the built-in function.

function do_shift(l) {
  var l0=l[0];
  for (var i=0; i<l.length-1; i++) {
    l[i] = l[i+1];
  }
  l.length = l.length - 1;
  return l0;
}

function go_to (url) {
    // XXX specifying "sidebar" here indicates you want to use a
    // function to do the actual loading instead of using the specified
    // url directly. bug 236025 covers clarifying this. Pages that specify
    // sidebar=1 *must* specify a load_absolute_url function meanwhile.
    if ( typeof sidebar != "undefined" && sidebar == 1 ) {
        load_absolute_url(url);
    } else {
        document.location.href = url;
    }
}

function map(l, f) {
    var l1 = new Array();
    for (var i=0; i<l.length; i++) {
        l1[i] = f(l[i]);
    }
    return l1;
}

function isPrefix(s1, s2) {
    return (s1.length <= s2.length) &&
           (s1 == s2.substring(0,s1.length))
}

function member(s, l) {
    for (var i=0; i<l.length; i++) {
        if (l[i] == s) return true;
    }
    return false;
}

function add(s, l) {
    if (! member(s, l)) {
        do_unshift(l,s);
    }
}

function addAll(l1, l2) {
    for (var i=0; i<l1.length; i++) {
        add(l1[i],l2);
    }        
}

function isSubset (l1, l2) {
    return (l1.length == 0)
    || (member(l1[0],l2) && subset(l1.slice(1),l2));
}

// fields

var f1 = new Array();
var f2 = new Array();

function add_mapping(from,to) {
    f1[f1.length] = from;
    f2[f2.length] = to;
}

// Status, Resolution, Platform, OS, Priority, Severity
add_mapping("status",             "bug_status");
add_mapping("resolution",         "resolution");  // no change
add_mapping("platform",           "rep_platform");
add_mapping("os",                 "op_sys");
add_mapping("opsys",              "op_sys");
add_mapping("priority",           "priority");    // no change
add_mapping("pri",                "priority");
add_mapping("severity",           "bug_severity");
add_mapping("sev",                "bug_severity");
// People: AssignedTo, Reporter, QA Contact, CC, Added comment (?)
add_mapping("owner",              "assigned_to");
add_mapping("assignee",           "assigned_to");
add_mapping("assignedto",         "assigned_to");
add_mapping("reporter",           "reporter");    // no change
add_mapping("rep",                "reporter");
add_mapping("qa",                 "qa_contact"); 
add_mapping("qacontact",          "qa_contact");
add_mapping("cc",                 "cc");          // no change
// Product, Version, Component, Target Milestone
add_mapping("product",            "product");     // no change
add_mapping("prod",               "product");
add_mapping("version",            "version");     // no change
add_mapping("ver",                "version");
add_mapping("component",          "component");   // no change
add_mapping("comp",               "component");
add_mapping("milestone",          "target_milestone");
add_mapping("target",             "target_milestone");
add_mapping("targetmilestone",    "target_milestone");
// Summary, Description, URL, Status whiteboard, Keywords
add_mapping("summary",            "short_desc");
add_mapping("shortdesc",          "short_desc");
add_mapping("desc",               "longdesc");
add_mapping("description",        "longdesc");
//add_mapping("comment",          "longdesc");    // ???
          // reserve "comment" for "added comment" email search?
add_mapping("longdesc",           "longdesc");
add_mapping("url",                "bug_file_loc");
add_mapping("whiteboard",         "status_whiteboard");
add_mapping("statuswhiteboard",   "status_whiteboard");
add_mapping("sw",                 "status_whiteboard");
add_mapping("keywords",           "keywords");    // no change
add_mapping("kw",                 "keywords");
// Attachments
add_mapping("attachment",         "attachments.description");
add_mapping("attachmentdesc",     "attachments.description");
add_mapping("attachdesc",         "attachments.description");
add_mapping("attachmentdata",     "attachments.thedata");
add_mapping("attachdata",         "attachments.thedata");
add_mapping("attachmentmimetype", "attachments.mimetype");
add_mapping("attachmimetype",     "attachments.mimetype");

// disabled because of bug 30823:
// "BugsThisDependsOn"       --> "dependson"
// "OtherBugsDependingOnThis"--> "blocked"
//add_mapping("dependson",        "dependson"); 
//add_mapping("blocked",          "blocked");

// Substring search doesn't make much sense for the following fields: 
// "Attachment is patch"    --> "attachments.ispatch"
// "Last changed date"      --> "delta_ts"
// "Days since bug changed" --> "(to_days(now()) - to_days(bugs.delta_ts))"
//"groupset"
//"everconfirmed"
//"bug","bugid","bugno"     --> "bug_id"
// "votes"                  --> "votes"
//     "votes>5", "votes>=5", "votes=>5" works now, see below
//     "votes:5" is interpreted as "votes>=5"

function findIndex(array,value) {
    for (var i=0; i<array.length; i++)
        if (array[i] == value) return i;
    return -1;
}

function mapField(fieldname) {
    var i = findIndex(f1,fieldname);
    if (i >= 0) return f2[i];
    return no_result;
} 

// `keywords' is defined externally
 
function is_keyword(s) {
    return member(s, keywords);
}

// `platforms' is defined externally

function is_platform(str) {
    return member (str.toLowerCase(),platforms);
}

// `severities' is defined externally

function is_severity(str) {
    return member(str.toLowerCase(),severities);
}

// `product_exceptions' is defined externally

function match_product(str) {
    var s = str.toLowerCase();
    return (s.length > 2) && (! member(s,product_exceptions));
}

// `component_exceptions are defined externally

function match_component(str) {
    var s = str.toLowerCase();
    return (s.length > 2) && (! member(s,component_exceptions));
}

var status_and_resolution = ""; // for pretty debug output only; these vars
var charts = "";                // always hold the data from the last query

// derived from http://www.mozilla.org/quality/help/bugreport.js

function make_chart(expr, field, type, value) {
    charts += "<tr>" +
              "<td><tt>" + expr + "</tt></td>" + 
              "<td><tt>" + field + "</tt></td>" + 
              "<td><tt>" + type + "</tt></td>" + 
              "<td><tt>" + value + "</tt></td>" +
              "</tr>";
    return "&field" + expr + "=" + field +
           "&type"  + expr + "=" + type  +
           "&value" + expr + "=" + escape(value).replace(/[+]/g,"%2B");
}

// returns true if at least one of comparelist had the prefix, false otherwise
function addPrefixMatches(prefix, comparelist, resultlist) {
    var foundMatch = false;
    for (var i=0; i<comparelist.length; i++) {
        if (isPrefix(prefix,comparelist[i])) {
            foundMatch = true;
            add(comparelist[i],resultlist);
        }
    }
    return foundMatch;
}

function prefixesNotFoundError(prefixes,statusValues,resolutionValues) {
    var txt;
    if (prefixes.length == 1) {
        txt = "is not a prefix ";
    } else {
        txt = "are not prefixes ";
    }
    alert(prefixes + "\n" + txt + 
          "of one of these status or resolution values:\n" +
          statusValues + "\n" + resolutionValues + "\n");
}

function make_query_URL(url, input, searchLong) {

    status_and_resolution = "";
    charts = "";

    // declare all variables used in this function
    
    var searchURL = url;  // bugzilla + "buglist.cgi" (or "query.cgi")
    var abort = false;    // global flag, checked upon return
   
    var i,j,k,l;          // index counters used in 'for' loops
    var parts,input2;     // escape "quoted" parts of input

    var word;                  // array of words 
                               //  (space-separated parts of input2)
    var alternative;           // array of parts of an element of 'word'
                               //  (separated by '|', sometimes by comma)
    var comma_separated_words; // array of parts of an element of 'alternative'
    var w;                     // current element of one of these arrays:
                               //  word, alternative, comma_separated_words
    
    var w0;               // first element of 'word'
    var prefixes;         // comma-separated parts of w0 
                          //  (prefixes of status/resolution values)

    var expr;             // used for 'priority' support
    var n,separator;      // used for 'votes' support
 
    var colon_separated_parts, fields,values,field;
                          // used for generic fields:values notation

    var chart,and,or;     // counters used in add_chart
    var negation;         // boolean flag used in add_chart

    // `statuses_open' and `statuses_resolved' are defined externally
    var statusOpen     = statuses_open;
    var statusResolved = statuses_resolved;
    var statusAll      = statusOpen.concat(statusResolved);

    // `resolutions' is defined externally
    var bug_status = statusOpen.slice().reverse(); //reverse is just cosmetic
    var resolution = new Array();
    
    // escape everything between quotes: "foo bar" --> "foo%20bar"
    parts = input.split('"');
    if ((parts.length % 2) != 1) {
        alert('Unterminated quote');
        abort = true;
        return no_result;      
    }
    for (i=1; i<parts.length; i+=2) {
        parts[i] = escape(parts[i]);
    }
    input2 = parts.join('"');

    // abort if there are still brackets
    if (input2.match(/[(]|[\)]/)) {
        alert('Brackets (...) are not supported.\n' + 
              'Use quotes "..." for values that contain special characters.');
        abort = true;
        return no_result;
    }

    // translate " AND "," OR "," NOT " to space,comma,dash
    input2 = input2.replace(/[\s]+AND[\s]+/g," ");
    input2 = input2.replace(/[\s]+OR[\s]+/g,"|");
    input2 = input2.replace(/[\s]+NOT[\s]+/g," -");

    // now split into words at space positions
    word = input2.split(/[\s]+/);

    // determine bug_status and resolution 
    // the first word may contain relevant info

    // This function matches the given prefixes against the given statuses and
    // resolutions. Matched statuses are added to bug_status, matched 
    // resolutions are added to resolution. Returns true if and only if
    // some matches were found for at least one of the given prefixes.
    function matchPrefixes(prefixes,statuses,resolutions) {
        var failedPrefixes = new Array();
        var foundMatch = false;
        for (var j=0; j<prefixes.length; j++) {
            var ok1 = addPrefixMatches(prefixes[j],statuses,bug_status);
            var ok2 = addPrefixMatches(prefixes[j],resolutions,resolution);
            if ((! ok1) && (! ok2)) {
                add(prefixes[j],failedPrefixes);
            } else {
                foundMatch = true;
            }
        }
        //report an error if some (but not all) prefixes didn't match anything
        if (foundMatch && (failedPrefixes.length > 0)) {
            prefixesNotFoundError(failedPrefixes,statuses,resolutions);
            abort = true;
        }
        return foundMatch;
    }
    
    if (word[0] == "ALL") {
        // special case: search for bugs regardless of status
        addAll(statusResolved,bug_status);
        do_shift(word);
    } else if (word[0] == "OPEN") {
        // special case: search for open bugs only
        do_shift(word);
    } else if (word[0].match("^[+][A-Z]+(,[A-Z]+)*$")) {
        // e.g. +DUP,FIX 
        w0 = do_shift(word);
        prefixes = w0.substring(1).split(",");
        if (! matchPrefixes(prefixes,statusResolved,resolutions)) {
            do_unshift(word,w0);
        }
    } else if (word[0].match("^[A-Z]+(,[A-Z]+)*$")) {
        // e.g. NEW,ASSI,REOP,FIX
        bug_status = new Array(); // reset
        w0 = do_shift(word);
        prefixes = w0.split(",");
        if (! matchPrefixes(prefixes,statusAll,resolutions)) {
            do_unshift(word,w0);
            bug_status = statusOpen.reverse(); //reset to default bug_status
        }
    } else {
        // default case: 
        // search for unresolved bugs only
        // uncomment this to include duplicate bugs in the search
        // add("DUPLICATE",resolution);
    }
    if (resolution.length > 0) {
        resolution = resolution.reverse();
        do_unshift(resolution,"---");
        addAll(statusResolved,bug_status);
    }
    bug_status = bug_status.reverse();
    bug_status = map(bug_status,escape);
    searchURL += "?bug_status=" +  bug_status.join("&bug_status=");
    status_and_resolution += 'Status: <tt>'+bug_status+'</tt>';

    if (resolution.length > 0) {
        resolution = map(resolution,escape);
        searchURL += "&resolution=" + resolution.join("&resolution=");
        status_and_resolution += '<br>'+'Resolution: <tt>'+resolution+'</tt>';
    }
                              
    // end of bug_status & resolution stuff

    chart = 0;
    and   = 0;
    or    = 0;

    negation = false;

    function negate_comparison_type(type) {
        switch(type) {
            case "substring": return "notsubstring";
            case "anywords":  return "nowords";
            case "regexp":    return "notregexp";
            default:
                // e.g. "greaterthan" 
                alert("Can't negate comparison type: `" + type + "'");
                abort = true;
                return "dummy";
        }
    }

    function add_chart(field,type,value) {
        // undo escaping for value: '"foo%20bar"' --> 'foo bar'
        var parts = value.split('"');
        if ((parts.length % 2) != 1) {
            alert('Internal error: unescaping failure');
            abort = true;
        }
        for (var i=1; i<parts.length; i+=2) {
            parts[i] = unescape(parts[i]);
        }
        var value2 = parts.join('');

        // negate type if negation is set
        var type2 = type;
        if (negation) {
            type2 = negate_comparison_type(type2);
        }
        searchURL += make_chart(chart+"-"+and+"-"+or,field,type2,value2);
        or++;
        if (negation) {
            and++;
            or=0;
        }
    }

    for (i=0; i<word.length; i++, chart++) {

        w = word[i];
        
        negation = false;
        if (w.charAt(0) == "-") {
            negation = true;
            w = w.substring(1);
        }

        switch (w.charAt(0)) {
            case "+":
                alternative = w.substring(1).split(/[|,]/);
                for (j=0; j<alternative.length; j++)
                    add_chart("short_desc","substring",alternative[j]);
                break;
            case "#":
                alternative = w.substring(1).replace(/[|,]/g," ");
                add_chart("short_desc","anywords",alternative);
                if (searchLong)
                    add_chart("longdesc","anywords",alternative);
                break;
            case ":":
                alternative = w.substring(1).split(",");
                for (j=0; j<alternative.length; j++) {
                    add_chart("product","substring",alternative[j]);
                    add_chart("component","substring",alternative[j]);
                }
                break;
            case "@":
                alternative = w.substring(1).split(",");
                for (j=0; j<alternative.length; j++)
                    add_chart("assigned_to","substring",alternative[j]);
                break;
            case "[":
                add_chart("short_desc","substring",w);
                add_chart("status_whiteboard","substring",w);
                break;
            case "!":
                add_chart("keywords","anywords",w.substring(1));
                break;
            default:
                alternative=w.split("|");
                for (j=0; j<alternative.length; j++) {

                    w=alternative[j];

                    // votes:xx ("at least xx votes")
                    if (w.match("^votes[:][0-9]+$")) {
                        n = w.split(/[:]/)[1];
                        add_chart("votes","greaterthan",String(n-1));
                        continue;
                    }
                    // generic field1,field2,field3:value1,value2 notation
                    if (w.match("^[^:]+[:][^:\/][^:]*$")) {
                        colon_separated_parts = w.split(":");
                        fields = colon_separated_parts[0].split(/[,]+/);
                        values = colon_separated_parts[1].split(/[,]+/);
                        for (k=0; k<fields.length; k++) {
                            field = mapField(fields[k]);
                            if (field == no_result) {
                                alert("`"+fields[k]+"'"+
                                      " is not a valid field name.");
                                abort = true;
                                return no_result;
                            } else {
                                 for (l=0; l<values.length; l++) {
                                     add_chart(field,"substring",values[l]);
                                 }
                            }  
                        }
                        continue;
                    }
                    comma_separated_words=w.split(/[,]+/);
                    for (k=0; k<comma_separated_words.length; k++) {
                        w=comma_separated_words[k];

                        // platform
                        if (is_platform(w)) {
                            add_chart("rep_platform","substring",w);
                            continue;
                        }
                        // priority
                        if (w.match("^[pP][1-5](,[pP]?[1-5])*$")) {
                            expr = "["+w.replace(/[p,]/g,"")+"]";
                            add_chart("priority","regexp",expr);
                            continue;
                        }
                        if (w.match("^[pP][1-5]-[1-5]$")) {
                            expr = "["+w.substring(1)+"]";
                            add_chart("priority","regexp",expr);
                            continue;
                        }
                        // severity
                        if (is_severity(w)) {
                            add_chart("bug_severity","substring",w);
                            continue;
                        }
                        // votes>xx
                        if (w.match("^votes>[0-9]+$")) {
                            n = w.split(">")[1];
                            add_chart("votes","greaterthan",n);
                            continue;
                        }
                        // votes>=xx, votes=>xx
                        if (w.match("^votes(>=|=>)[0-9]+$")) {
                            separator = w.match("^votes(>=|=>)[0-9]+$")[1];
                            n = w.split(separator)[1];
                            add_chart("votes","greaterthan",String(n-1));
                            continue;
                        }
                        // really default case
                        if (match_product(w)) {
                            add_chart("product","substring",w);
                        }
                        if (match_component(w)) {
                            add_chart("component","substring",w);
                        }
                        if (is_keyword(w)) {
                            add_chart("keywords","substring",w);
                            if (w.length > 2) {
                                add_chart("short_desc","substring",w);
                                add_chart("status_whiteboard","substring",w);
                            }
                        } else {
                            add_chart("short_desc","substring",w);
                            add_chart("status_whiteboard","substring",w);
                        }
                        if (searchLong)
                            add_chart("longdesc","substring",w);
                 
                        // URL field (for IP addrs, host.names, scheme://urls)
                        if (w.match(/[0-9]+[.][0-9]+[.][0-9]+[.][0-9]+/)
                           || w.match(/^[A-Za-z]+([.][A-Za-z]+)+/)
                           || w.match(/[:][\/][\/]/)
                           || w.match(/localhost/)
                           || w.match(/mailto[:]?/)
                           // || w.match(/[A-Za-z]+[:][0-9]+/) //host:port
                           )
                            add_chart("bug_file_loc","substring",w);
                    }
                }
        }
        and = 0;
        or = 0;
    }

    //searchURL += "&cmdtype=doit";

    if (abort == false) {
        return searchURL;
    } else {
        return no_result;
    }
}

function unique_id () {
    return (new Date()).getTime();
}

function ShowURL(mode,input) {
    var searchURL = make_query_URL(bugzilla+"buglist.cgi", input, false);
    if (searchURL != no_result) {
        var pieces = searchURL.replace(/[\?]/g,"\n?").replace(/[\&]/g,"\n&");
        if (mode == "alert") {
            alert(pieces);
        } else {
            var table = "<table border=1>" + 
                          "<thead>" + 
                            "<tr>" + 
                              "<th>Chart-And-Or</th>" + 
                              "<th>Field</th>" + 
                              "<th>Type</th>" + 
                              "<th>Value</th>" + 
                            "</tr>" + 
                          "</thead>" + 
                          "<tbody>" + charts + "</tbody>" +
                        "</table>";
            var html = '<html>' + 
                         '<head>' + 
                           '<title>' + input + '</title>' +
                         '</head>' +
                         '<body>' + 
                           '<a href="' + searchURL + '">' +
                             'Submit Query' +
                           '</a>' +
                           '<p>' + status_and_resolution + 
                           '<p>' + table + 
                           '<pre>' +
                             pieces.replace(/[\n]/g,"<br>") +
                           '</pre>' +  
                         '</body>' +
                       '</html>';
            var w = window.open("","preview_"+unique_id());
            w.document.write(html);
            w.document.close();
        }
    }
}

//
// new interface: 
// searchLong is a boolean now (not a checkbox/radiobutton)
//
function Search(url, input, searchLong) {
    var inputstring = new String(input);
    var word = inputstring.split(/[\s]+/);
  
    // Check for empty input
    if ( word.length == 1 && word[0] == "" )
        return;
    
    // Check for potential Bugzilla-busting intensive queries
    if ((searchLong!=false) && word.length > 4) {  
        var message = "Searching Descriptions for more than four words " +
                      "will take a very long time indeed. Please choose " +
                      "no more than four keywords for your query.";
        alert(message);
        return;
    }
    var searchURL = make_query_URL(url, inputstring, searchLong);
    if (searchURL != no_result) {
        go_to(searchURL);
         //window.open(searchURL, "other" );
    } else {
        return;
    }
}

//
// original interface, untested
//
//function SearchForBugs (input, searchRadio) {
//    if (searchRadio[0].checked) {
//        return Search(bugzilla + "buglist.cgi", input, false);
//    } else {
//        return Search(bugzilla + "buglist.cgi", input, true);
//    }
//}

// derived from http://www.cs.hmc.edu/~jruderma/s/bugz.html

// QuickSearch combines lookup-by-bug-number and search
// in a single textbox. 
//
// type nothing:
//    --> go to bugzilla front page
// type a number:
//    --> go to that bug number
// type several numbers, separated by commas:
//    --> go to a buglist of just those bug numbers
// type anything else:
//    --> search summary, product, component, keywords, status whiteboard
//        (and URL if it's an IP address, a host.name, or an absolute://URL)

function QuickSearch (input)
{
    //remove leading and trailing whitespace
    input = input.replace(/^[\s]+/,"").replace(/[\s]+$/,"");

    if (input == "") 
    {
        //once this _is_ on http://bugzilla.mozilla.org, it should just return;
        go_to(bugzilla);
    } 
    else if (input.match(/^[0-9, ]*$/)) 
    {
        if (input.indexOf(",") == -1) {
            // only _one_ bug number --> show_bug
            go_to(bugzilla+"show_bug.cgi?id="+escape(input));
        } else {
            // comma-separated bug numbers --> buglist
            go_to(bugzilla+"buglist.cgi?bug_id="+escape(input)
                  + "&bugidtype=include&order=bugs.bug_id");
        }
    }
    else
    {
        Search(bugzilla+"buglist.cgi",input,false);
    }
    return;
}

function LoadQuery(input) {
    //remove leading and trailing whitespace
    input = input.replace(/^[\s]+/,"").replace(/[\s]+$/,"");

    Search(bugzilla+"query.cgi",input,false);
    return;
}

