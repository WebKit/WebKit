require 'cgi'
require 'diff'
require 'open3'
require 'pp'
require 'set'
require 'tempfile'

module PrettyPatch

public

    GIT_PATH = "git"

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

    BINARY_FILE_MARKER_FORMAT = /^Cannot display: file marked as a binary type.$/

    IMAGE_FILE_MARKER_FORMAT = /^svn:mime-type = image\/png$/

    GIT_INDEX_MARKER_FORMAT = /^index ([0-9a-f]{40})\.\.([0-9a-f]{40})/

    GIT_BINARY_FILE_MARKER_FORMAT = /^GIT binary patch$/

    GIT_LITERAL_FORMAT = /^literal \d+$/

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
        WebKit2
        WebKitExamplePlugins
        WebKitLibraries
        WebKitSite
        WebKitTools
        autotools
        cmake
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

/* Support for inline comments */

.author {
  font-style: italic;
}

.comment {
  position: relative;
}

.comment textarea, .overallComments textarea {
  width: 100%;
  height: 6em;
}

body {
  margin-bottom: 40px;
}

#toolbar {
  position: fixed;
  padding: 5px;
  bottom: 0;
  left: 0;
  right: 0;
  border-top: 1px solid #ddd;
  background-color: #eee;
}

#toolbar .actions {
  float: left;
}

#toolbar .message {
  float: right;
}

#toolbar .commentStatus {
  font-style:italic
}

.winter {
  position: fixed;
  z-index: 5;
  left: 0;
  right: 0;
  top: 0;
  bottom: 0;
  background-color: black;
  opacity: 0.8;
}

.inactive {
  display: none;
}

.lightbox {
  position: fixed;
  z-index: 6;
  left: 10%;
  right: 10%;
  top: 10%;
  bottom: 10%;
}

.lightbox iframe {
  width: 100%;
  height: 100%;
}

.commentContext .lineNumber {
  background-color: yellow;
}

.selected .lineNumber {
  background-color: #69F;
  border-bottom-color: #69F;
  border-right-color: #69F;
}

.help {
 color: gray;
 font-style: italic;
}

.description {
  font-style: italic;
}

.comment, .overallComments, .previousComment, .frozenComment {
  background-color: #ffd;
}

.overallComments {
  padding: 5px;
}

.previousComment, .frozenComment {
  border: inset 1px;
  padding: 5px;
  white-space: pre-wrap;
}

.comment button {
  width: 6em;
}

.focused {
  border: 1px solid blue;
}

.statusBubble {
  margin-left: 5px;
  width: 175px;
  height: 20px;
  border: none;
  vertical-align: middle;
}
</style>
<script src="https://ajax.googleapis.com/ajax/libs/jquery/1.4.2/jquery.min.js"></script> 
<script src="code-review.js?version=9"></script> 
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
                when GIT_INDEX_MARKER_FORMAT
                    @git_indexes = [$1, $2]
                when GIT_BINARY_FILE_MARKER_FORMAT
                    @binary = true
                    if (GIT_LITERAL_FORMAT.match(lines[i + 1]) and PrettyPatch.has_image_suffix(@filename)) then
                        @git_image = true
                        startOfSections = i + 1
                    end
                    break
                end
            end
            lines_with_contents = lines[startOfSections...lines.length]
            @sections = DiffSection.parse(lines_with_contents) unless @binary
            if @image
                @image_url = "data:image/png;base64," + lines_with_contents.join
            elsif @git_image
                begin
                    raise "index line is missing" unless @git_indexes

                    chunks = nil
                    for i in 0...lines_with_contents.length
                        if lines_with_contents[i] =~ /^$/
                            chunks = [lines_with_contents[i + 1 .. -1], lines_with_contents[0 .. i]]
                            break
                        end
                    end

                    raise "no binary chunks" unless chunks

                    @image_urls = chunks.zip(@git_indexes).collect do |chunk, git_index|
                        FileDiff.extract_contents_from_git_binary_chunk(chunk, git_index)
                    end
                rescue
                    @image_error = "Exception raised during decoding git binary patch:<pre>#{CGI.escapeHTML($!.to_s + "\n" + $!.backtrace.join("\n"))}</pre>"
                end
            end
            nil
        end

        def to_html
            str = "<div class='FileDiff'>\n"
            str += "<h1>#{PrettyPatch.linkifyFilename(@filename)}</h1>\n"
            if @image then
                str += "<img class='image' src='" + @image_url + "' />"
            elsif @git_image then
                if @image_error
                    str += @image_error
                else
                    for i in (0...2)
                        image_url = @image_urls[i]
                        style = ["remove", "add"][i]
                        str += "<p class=\"#{style}\">"
                        if image_url
                            str += "<img class='image' src='" + image_url + "' />"
                        else
                            str += ["Added", "Removed"][i]
                        end
                    end
                end
            elsif @binary then
                str += "<span class='text'>Binary file, nothing to see here</span>"
            else
                str += @sections.collect{ |section| section.to_html }.join("<br>\n") unless @sections.nil?
            end
            str += "</div>\n"
        end

        def self.parse(string)
            haveSeenDiffHeader = false
            linesForDiffs = []
            string.each_line do |line|
                if (PrettyPatch.diff_header?(line))
                    linesForDiffs << []
                    haveSeenDiffHeader = true
                elsif (!haveSeenDiffHeader && line =~ /^--- /)
                    linesForDiffs << []
                    haveSeenDiffHeader = false
                end
                linesForDiffs.last << line unless linesForDiffs.last.nil?
            end

            linesForDiffs.collect { |lines| FileDiff.new(lines) }
        end

        def self.git_new_file_binary_patch(filename, encoded_chunk, git_index)
            return <<END
diff --git a/#{filename} b/#{filename}
new file mode 100644
index 0000000000000000000000000000000000000000..#{git_index}
GIT binary patch
#{encoded_chunk.join("")}literal 0
HcmV?d00001

END
        end

        def self.extract_contents_from_git_binary_chunk(encoded_chunk, git_index)
            # We use Tempfile we need a unique file among processes.
            tempfile = Tempfile.new("PrettyPatch")
            # We need a filename which doesn't exist to apply a patch
            # which creates a new file. Append a suffix so filename
            # doesn't exist.
            filepath = tempfile.path + '.bin'
            filename = File.basename(filepath)

            patch = FileDiff.git_new_file_binary_patch(filename, encoded_chunk, git_index)

            # Apply the git binary patch using git-apply.
            cmd = GIT_PATH + " apply --directory=" + File.dirname(filepath)
            stdin, stdout, stderr = *Open3.popen3(cmd)
            begin
                stdin.puts(patch)
                stdin.close

                error = stderr.read
                raise error if error != ""

                contents = File.read(filepath)
            ensure
                stdin.close unless stdin.closed?
                stdout.close
                stderr.close
                File.unlink(filename) if File.exists?(filename)
            end

            return nil if contents.empty?
            return "data:image/png;base64," + [contents].pack("m")
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
