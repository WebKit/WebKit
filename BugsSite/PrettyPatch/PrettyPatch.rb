require 'cgi'
require 'diff'
require 'pp'
require 'set'

module PrettyPatch

public

    def self.prettify(string)
        fileDiffs = FileDiff.parse(string)

        str = HEADER + "\n"
        str += fileDiffs.collect{ |diff| diff.to_html }.join
    end

    def self.filename_from_diff_header(line)
        DIFF_HEADER_FORMATS.each do |format|
            match = format.match(line)
            return match[1] unless match.nil?
        end
        nil
    end

    def self.diff_header?(line)
        RELAXED_DIFF_HEADER_FORMATS.any? { |format| line =~ format }
    end

private
    DIFF_HEADER_FORMATS = [
        /^Index: (.*)\r?$/,
        /^diff --git "?a\/.+"? "?b\/(.+)"?\r?$/,
        /^\+\+\+ ([^\t]+)(\t.*)?\r?$/
    ]

    RELAXED_DIFF_HEADER_FORMATS = [
        /^Index:/,
        /^diff/
    ]

    BINARY_FILE_MARKER_FORMAT = /^(?:Cannot display: file marked as a binary type.)|(?:GIT binary patch)$/

    IMAGE_FILE_MARKER_FORMAT = /^svn:mime-type = image\/png$/

    START_OF_BINARY_DATA_FORMAT = /^[0-9a-zA-Z\+\/=]{20,}/ # Assume 20 chars without a space is base64 binary data.

    START_OF_SECTION_FORMAT = /^@@ -(\d+)(?:,\d+)? \+(\d+)(?:,\d+)? @@\s*(.*)/

    START_OF_EXTENT_STRING = "%c" % 0
    END_OF_EXTENT_STRING = "%c" % 1

    SMALLEST_EQUAL_OPERATION = 3

    OPENSOURCE_TRAC_URL = "http://trac.webkit.org/"

    OPENSOURCE_DIRS = Set.new %w[
        BugsSite
        JavaScriptCore
        JavaScriptGlue
        LayoutTests
        PageLoadTests
        PlanetWebKit
        SunSpider
        WebCore
        WebKit
        WebKitExamplePlugins
        WebKitLibraries
        WebKitSite
        WebKitTools
    ]

    def self.find_url_and_path(file_path)
        # Search file_path from the bottom up, at each level checking whether
        # we've found a directory we know exists in the source tree.

        dirname, basename = File.split(file_path)
        dirname.split(/\//).reverse.inject(basename) do |path, directory|
            path = directory + "/" + path

            return [OPENSOURCE_TRAC_URL, path] if OPENSOURCE_DIRS.include?(directory)

            path
        end

        [nil, file_path]
    end

    def self.linkifyFilename(filename)
        url, pathBeneathTrunk = find_url_and_path(filename)

        url.nil? ? filename : "<a href='#{url}browser/trunk/#{pathBeneathTrunk}'>#{filename}</a>"
    end


    HEADER =<<EOF
<style>
:link, :visited {
    text-decoration: none;
    border-bottom: 1px dotted;
}

.FileDiff {
    background-color: #f8f8f8;
    border: 1px solid #ddd;
    font-family: monospace;
    margin: 2em 0px;
}

h1 {
    color: #333;
    font-family: sans-serif;
    font-size: 1em;
    margin-left: 0.5em;
}

h1 :link, h1 :visited {
    color: inherit;
}

h1 :hover {
    color: #555;
    background-color: #eee;
}

.DiffSection {
    background-color: white;
    border: solid #ddd;
    border-width: 1px 0px;
}

.lineNumber {
    background-color: #eed;
    border-bottom: 1px solid #998;
    border-right: 1px solid #ddd;
    color: #444;
    display: inline-block;
    padding: 1px 5px 0px 0px;
    text-align: right;
    vertical-align: bottom;
    width: 3em;
}

.text {
    padding-left: 5px;
    white-space: pre;
    white-space: pre-wrap;
}

.image {
    border: 2px solid black;
}

.context, .context .lineNumber {
    color: #849;
    background-color: #fef;
}

.add {
    background-color: #dfd;
}

.add ins {
    background-color: #9e9;
    text-decoration: none;
}

.remove {
    background-color: #fdd;
}

.remove del {
    background-color: #e99;
    text-decoration: none;
}
</style>
EOF

    def self.revisionOrDescription(string)
        case string
        when /\(revision \d+\)/
            /\(revision (\d+)\)/.match(string)[1]
        when /\(.*\)/
            /\((.*)\)/.match(string)[1]
        end
    end

    def self.has_image_suffix(filename)
        filename =~ /\.(png|jpg|gif)$/
    end

    class FileDiff
        def initialize(lines)
            @filename = PrettyPatch.filename_from_diff_header(lines[0].chomp)
            startOfSections = 1
            for i in 0...lines.length
                case lines[i]
                when /^--- /
                    @from = PrettyPatch.revisionOrDescription(lines[i])
                when /^\+\+\+ /
                    @filename = PrettyPatch.filename_from_diff_header(lines[i].chomp) if @filename.nil?
                    @to = PrettyPatch.revisionOrDescription(lines[i])
                    startOfSections = i + 1
                    break
                when BINARY_FILE_MARKER_FORMAT
                    @binary = true
                    if (IMAGE_FILE_MARKER_FORMAT.match(lines[i + 1]) or PrettyPatch.has_image_suffix(@filename)) then
                        @image = true
                        startOfSections = i + 2
                        for x in startOfSections...lines.length
                            # Binary diffs often have property changes listed before the actual binary data.  Skip them.
                            if START_OF_BINARY_DATA_FORMAT.match(lines[x]) then
                                startOfSections = x
                                break
                            end
                        end
                    end
                    break
                end
            end
            lines_with_contents = lines[startOfSections...lines.length]
            @sections = DiffSection.parse(lines_with_contents) unless @binary
            @image_url = "data:image/png;base64," + lines_with_contents.join if @image
            nil
        end

        def to_html
            str = "<div class='FileDiff'>\n"
            str += "<h1>#{PrettyPatch.linkifyFilename(@filename)}</h1>\n"
            if @image then
                str += "<img class='image' src='" + @image_url + "' />"
            elsif @binary then
                str += "<span class='text'>Binary file, nothing to see here</span>"
            else
                str += @sections.collect{ |section| section.to_html }.join("<br>\n") unless @sections.nil?
            end
            str += "</div>\n"
        end

        def self.parse(string)
            haveSeenDiffHeader = false
            linesForDiffs = string.inject([]) do |diffChunks, line|
                if (PrettyPatch.diff_header?(line))
                    diffChunks << []
                    haveSeenDiffHeader = true
                elsif (!haveSeenDiffHeader && line =~ /^--- /)
                    diffChunks << []
                    haveSeenDiffHeader = false
                end
                diffChunks.last << line unless diffChunks.last.nil?
                diffChunks
            end

            linesForDiffs.collect { |lines| FileDiff.new(lines) }
        end
    end

    class DiffSection
        def initialize(lines)
            lines.length >= 1 or raise "DiffSection.parse only received %d lines" % lines.length

            matches = START_OF_SECTION_FORMAT.match(lines[0])
            from, to = [matches[1].to_i, matches[2].to_i] unless matches.nil?

            @lines = lines[1...lines.length].collect do |line|
                startOfLine = line =~ /^[-\+ ]/ ? 1 : 0
                text = line[startOfLine...line.length].chomp
                case line[0]
                when ?-
                    result = CodeLine.new(from, nil, text)
                    from += 1 unless from.nil?
                    result
                when ?+
                    result = CodeLine.new(nil, to, text)
                    to += 1 unless to.nil?
                    result
                else
                    result = CodeLine.new(from, to, text)
                    from += 1 unless from.nil?
                    to += 1 unless to.nil?
                    result
                end
            end

            @lines.unshift(ContextLine.new(matches[3])) unless matches.nil? || matches[3].empty?

            changes = [ [ [], [] ] ]
            for line in @lines
                if (!line.fromLineNumber.nil? and !line.toLineNumber.nil?) then
                    changes << [ [], [] ]
                    next
                end
                changes.last.first << line if line.toLineNumber.nil?
                changes.last.last << line if line.fromLineNumber.nil?
            end

            for change in changes
                next unless change.first.length == change.last.length
                for i in (0...change.first.length)
                    raw_operations = HTMLDiff::DiffBuilder.new(change.first[i].text, change.last[i].text).operations
                    operations = []
                    back = 0
                    raw_operations.each_with_index do |operation, j|
                        if operation.action == :equal and j < raw_operations.length - 1
                           length = operation.end_in_new - operation.start_in_new
                           if length < SMALLEST_EQUAL_OPERATION
                               back = length
                               next
                           end
                        end
                        operation.start_in_old -= back
                        operation.start_in_new -= back
                        back = 0
                        operations << operation
                    end
                    change.first[i].operations = operations
                    change.last[i].operations = operations
                end
            end
        end

        def to_html
            str = "<div class='DiffSection'>\n"
            str += @lines.collect{ |line| line.to_html }.join
            str += "</div>\n"
        end
        
        def self.parse(lines)
            linesForSections = lines.inject([[]]) do |sections, line|
                sections << [] if line =~ /^@@/
                sections.last << line
                sections
            end

            linesForSections.delete_if { |lines| lines.nil? or lines.empty? }
            linesForSections.collect { |lines| DiffSection.new(lines) }
        end
    end

    class Line
        attr_reader :fromLineNumber
        attr_reader :toLineNumber
        attr_reader :text

        def initialize(from, to, text)
            @fromLineNumber = from
            @toLineNumber = to
            @text = text
        end

        def text_as_html
            CGI.escapeHTML(text)
        end

        def classes
            lineClasses = ["Line"]
            lineClasses << ["add"] unless @toLineNumber.nil? or !@fromLineNumber.nil?
            lineClasses << ["remove"] unless @fromLineNumber.nil? or !@toLineNumber.nil?
            lineClasses
        end

        def to_html
            markedUpText = self.text_as_html
            str = "<div class='%s'>\n" % self.classes.join(' ')
            str += "<span class='from lineNumber'>%s</span><span class='to lineNumber'>%s</span>\n" %
                   [@fromLineNumber.nil? ? '&nbsp;' : @fromLineNumber,
                    @toLineNumber.nil? ? '&nbsp;' : @toLineNumber] unless @fromLineNumber.nil? and @toLineNumber.nil?
            str += "<span class='text'>%s</span>\n" % markedUpText
            str += "</div>\n"
        end
    end

    class CodeLine < Line
        attr :operations, true

        def text_as_html
            html = []
            tag = @fromLineNumber.nil? ? "ins" : "del"
            if @operations.nil? or @operations.empty?
                return CGI.escapeHTML(@text)
            end
            @operations.each do |operation|
                start = @fromLineNumber.nil? ? operation.start_in_new : operation.start_in_old
                eend = @fromLineNumber.nil? ? operation.end_in_new : operation.end_in_old
                escaped_text = CGI.escapeHTML(@text[start...eend])
                if eend - start === 0 or operation.action === :equal
                    html << escaped_text
                else
                    html << "<#{tag}>#{escaped_text}</#{tag}>"
                end
            end
            html.join
        end
    end

    class ContextLine < Line
        def initialize(context)
            super("@", "@", context)
        end

        def classes
            super << "context"
        end
    end
end
