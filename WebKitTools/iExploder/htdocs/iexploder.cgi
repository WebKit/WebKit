#!/usr/bin/ruby
# iExploder - Generates bad HTML files to perform QA for web browsers.
# Developed for the Mozilla Foundation.
#####################
#
# Copyright (c) 2005 Thomas Stromberg <thomas%stromberg.org>
# 
# This software is provided 'as-is', without any express or implied warranty.
# In no event will the authors be held liable for any damages arising from the
# use of this software.
# 
# Permission is granted to anyone to use this software for any purpose,
# including commercial applications, and to alter it and redistribute it
# freely, subject to the following restrictions:
# 
# 1. The origin of this software must not be misrepresented; you must not
# claim that you wrote the original software. If you use this software in a
# product, an acknowledgment in the product documentation would be appreciated
# but is not required.
# 
# 2. Altered source versions must be plainly marked as such, and must not be
# misrepresented as being the original software.
# 
# 3. This notice may not be removed or altered from any source distribution.
# 
# 4. Any bug reports, patches, or other publications which are based on the 
# usage of iExploder must reference iExploder as the source of the discovery.


require 'cgi';

$VERSION = "1.2";
$maxTags = 32;
$maxAttrs = 4;
$maxCSS = 5;
$mangledTagTotal = 0


def readTagFile(filename)
    list = Array.new
    File.new(filename).readlines.each { |line|
        line.chop!
        
        # Don't include comments.
        if (line !~ /^# /) && (line.length > 0)
            list << line
        end
    }
    return  list
end

# based on make_up_value, essentially.
def inventValue
    value = rand(19);
    case value
    when 1..3 then return ($htmlValues[rand($htmlValues.length)])
    when 4..5 then return ($htmlValues[rand($htmlValues.length)] + inventValue())
    when 6 then return ($htmlValues[rand($htmlValues.length)] + "//" + inventValue())
    when 7 then return ''
    when 8..10 then return rand(255).chr * (rand(256)+8)
    when 11 then return rand(255).chr * (rand(2048)+8)
    when 12 then return "#" + rand(999999).to_s
    when 13 then return rand(999999).to_s + "%"
    when 14..15 then return "&" + rand(999999).to_s + ";"
        # filters
    when 16 then
        return inventValue() + "=" + inventValue()
        
    when 17 then return inventValue() + "," + inventValue()
    else
        if rand(5) > 3
            return "-" + rand(999999).to_s
        else
            return rand(999999).to_s
        end
    end
end

# based on make_up_value, essentially.
def inventCssValue(tag)
    value = rand(23);
    case value
    when 1..10 then return $cssValues[rand($cssValues.length)]
    when 11 then return ''
    when 12 then return rand(255).chr * (rand(8192)+8)
    when 13
        length = rand(1024) + 8
        return (rand(255).chr * length) + " " + (rand(255).chr * length) + " " + (rand(255).chr * length)
    when 14 then return (rand(255).chr * (rand(1024)+3)) + "px"
    when 15 then return (rand(255).chr * (rand(1024)+3)) + "em"
    when 16 then return "url(" + inventValue() + ")"
    when 17..18 then return "#" + rand(999999999).to_s
    when 19 then return "-" + rand(99999999).to_s
    else return rand(99999999).to_s;
    end
end


def mangleTag(tag)
    $mangledTagTotal += 1
    out = ''
        
    # 20% chance of closing a tag instead of opening it.
    if rand(10) > 8
        out = "</" + tag + ">"
        return out
    end
    
    # we're opening it.
    out = "<" + tag
    
    # forgot the space between the tag and the attributes
    if rand(15) > 1
        out << ' '
    end
    
    attrNum = rand($maxAttrs) + 1
    
    1.upto(attrNum) {
        attr = $htmlAttr[rand($htmlAttr.length)]
        
        out << attr
        
        # 7.5% of the time we skip the = sign. Don't prefix it
        # if the attribute ends with a ( however.
        
        
        if rand(15) > 1
            out << '='
        end
        
        # sometimes quote it, sometimes not. I doubt the importance
        # of this test, but mangleme-1.2 added it, and adding more
        # random-ness never hurt anything but time. I'll do it less often.
        
        quote = rand(2)
        if (quote > 1)
            out << "\""
        end
        
        out << inventValue()
        
        # end the quote when you are done
        if (quote > 1)
            out << "\" "
        end
        
        # 5% chance we skip the space at the end of the name
        if rand(20) > 1
            out << ' '
        end
        
    }
    
    # CSS styles!
    if rand(4) > 1
        out << " style=\""
        1.upto(rand($maxCSS)+1) {
            out << $cssTags[rand($cssTags.length)]
            
            # very small chance we let the tag run on.
            if rand(50) > 1
                out << ": "
            end
            
            out << inventCssValue(tag)
            # we almost always put the ; there.
            if rand(50) > 1
                out << '; '
            end
        }
        out << "\""
    end
    
    # support our local troops!    
    if ($paramSubTestNum > 0) && ($paramSubTestNum != $mangledTagTotal) 
	if tag =~ /html|body|head/
		return '<' + tag + '>'
	else
            return '<subtest-ignored>'
	end
    end
    
    out << ">\n"
end

# These if statements are so that mod_ruby doesn't have to reload the files
# each time

if (! $cssTags)
    $cssTags = readTagFile('cssproperties.in');
end

if (! $htmlTags)
    $htmlTags = readTagFile('htmltags.in');
end
if (! $htmlAttr)
    $htmlAttr = readTagFile('htmlattrs.in');
end

if (! $htmlValues)
    $htmlValues = readTagFile('htmlvalues.in');
end

if (! $cssValues)
    $cssValues = readTagFile('cssvalues.in');
end

### THE INTERACTION ##################################
cgi = CGI.new("html4");

$paramTestNum = cgi.params['test'][0].to_i
$paramRandomMode = cgi.params['random'][0]
$paramLookupMode = cgi.params['lookup'][0]
$paramSubTestNum = cgi.params['subtest'][0].to_i || 0

if ($paramTestNum)
    srand($paramTestNum)
else
    srand
end

if $paramSubTestNum
    if $paramSubTestNum > $maxTags
        $paramSubTestNum = 0
        $paramLookupMode = 1
    else
        maxTags=$paramSubTestNum
    end
else
    maxTags=$maxTags
end

if $paramRandomMode
    nextTest = rand(99999999)
else
    if $paramTestNum
        nextTest = $paramTestNum + 1
    end
end


if (! nextTest)
    nextTest = 1
end

# building the HTML
bodyText = mangleTag('html')
bodyText << "\n<head>\n"

# Only do redirects if lookup=1 has not been specified.
if (! $paramLookupMode) && ($paramSubTestNum != $maxTags)
    newpage = "iexploder.cgi?"
    if $paramSubTestNum > 0
        newpage << "test=" << $paramTestNum.to_s << "&subtest=" << ($paramSubTestNum + 1).to_s
    else
        newpage << "test=" << nextTest.to_s
    end
    
    if $paramRandomMode
        newpage << "&random=1"
    end


    bodyText << "\t<META HTTP-EQUIV=\"Refresh\" content=\"0;URL=#{newpage}\">\n" 
    # use both techniques, because you never know how you might be corrupting yourself.
    bodyText << "\t<script language=\"javascript\">setTimeout('window.location=\"#{newpage}\"', 1000);</script>\n" 
end

# If they have no test number, don't crash them, just redirect them to the next test.
if (! $paramTestNum)
    cgi.out {
        cgi.html { bodyText }
    }
    exit
end

bodyText << "\t" << mangleTag('meta')
bodyText << "\t" <<  mangleTag('meta')
bodyText << "\t" <<  mangleTag('link')

bodyText << "\t<title>(#{cgi['test']}) iExploder #{$VERSION}  - #{inventValue()}</title>\n"
bodyText << "</head>\n\n"

# What tags will we be messing with ######################
tagList = [ 'body']

1.upto($maxTags) { tagList << $htmlTags[rand($htmlTags.length)] }


tagList.each { |tag|
    bodyText << mangleTag(tag)
    bodyText << inventValue() + "\n"
}
bodyText << "</body>\n</html>"

cgi.out('type' => 'text/html') do
    bodyText
end
