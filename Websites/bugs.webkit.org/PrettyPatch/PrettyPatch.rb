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

        # Just look at the first line to see if it is an SVN revision number as added
        # by webkit-patch for git checkouts.
        string.each_line do |line|
            match = /^Subversion\ Revision: (\d*)$/.match(line)
            unless match.nil?
              str += "<span class='revision'>" + match[1] + "</span>\n"
            end
            break
        end

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

    START_OF_SECTION_FORMAT = /^@@ -(\d+)(?:,(\d+))? \+(\d+)(?:,(\d+))? @@\s*(.*)/

    START_OF_EXTENT_STRING = "%c" % 0
    END_OF_EXTENT_STRING = "%c" % 1

    # We won't search for intra-line diffs in lines longer than this length, to avoid hangs. See <http://webkit.org/b/56109>.
    MAXIMUM_INTRALINE_DIFF_LINE_LENGTH = 10000

    SMALLEST_EQUAL_OPERATION = 3

    OPENSOURCE_TRAC_URL = "http://trac.webkit.org/"

    OPENSOURCE_DIRS = Set.new %w[
        Examples
        LayoutTests
        PerformanceTests
        Source
        Tools
        WebKit
        WebKitLibraries
        Websites
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

:link {
    color: #039;
}

.FileDiff {
    background-color: #f8f8f8;
    border: 1px solid #ddd;
    font-family: monospace;
    margin: 1em 0;
}

h1 {
    color: #333;
    font-family: sans-serif;
    font-size: 1em;
    margin-left: 0.5em;
    display: table-cell;
    width: 100%;
    padding: 0.5em;
}

h1 :link, h1 :visited {
    color: inherit;
}

h1 :hover {
    color: #555;
    background-color: #eee;
}

.DiffLinks {
    float: right;
}

.FileDiffLinkContainer {
    opacity: 0;
    display: table-cell;
    padding-right: 0.5em;
    white-space: nowrap;
}

.DiffSection {
    background-color: white;
    border: solid #ddd;
    border-width: 1px 0px;
}

.ExpansionLine, .LineContainer {
    white-space: nowrap;
}

.sidebyside .DiffBlockPart.add:first-child {
    float: right;
}

.LineSide:last-child {
    float: right;
}

.LineSide,
.sidebyside .DiffBlockPart.remove,
.sidebyside .DiffBlockPart.add {
    display:inline-block;
    width:50%;
    vertical-align: top;
}

.sidebyside .DiffBlockPart.remove .to,
.sidebyside .DiffBlockPart.add .from {
    display: none;
}

.lineNumber, .expansionLineNumber {
    border-bottom: 1px solid #998;
    border-right: 1px solid #ddd;
    color: #444;
    display: inline-block;
    padding: 1px 5px 0px 0px;
    text-align: right;
    vertical-align: bottom;
    width: 3em;
}

.lineNumber {
  background-color: #eed;
}

.expansionLineNumber {
  background-color: #eee;
}

.text {
    padding-left: 5px;
    white-space: pre-wrap;
    word-wrap: break-word;
}

.image {
    border: 2px solid black;
}

.context, .context .lineNumber {
    color: #849;
    background-color: #fef;
}

.Line.add {
    background-color: #dfd;
}

.Line.add ins {
    background-color: #9e9;
    text-decoration: none;
}

.Line.remove {
    background-color: #fdd;
}

.Line.remove del {
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

.comment textarea {
  height: 6em;
}

.overallComments textarea {
  height: 2em;
}

.comment textarea, .overallComments textarea {
  display: block;
  width: 100%;
}

.overallComments .open {
  -webkit-transition: height .2s;
  height: 4em;
}

#statusBubbleContainer.wrap {
  display: block;
}

body {
  margin-bottom: 40px;
}

#toolbar {
  display: -webkit-box;
  display: -moz-box;
  padding: 3px;
  left: 0;
  right: 0;
  border: 1px solid #ddd;
  background-color: #eee;
  font-family: sans-serif;
  position: fixed;
}

#toolbar.anchored {
  bottom: 0;
}

#toolbar .actions {
  float: right;
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
  background: white;
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

.ExpandLinkContainer {
  opacity: 0;
  border-top: 1px solid #ddd;
  border-bottom: 1px solid #ddd;
}

.ExpandArea {
  margin: 0;
}

.ExpandText {
  margin-left: 0.67em;
}

.LinkContainer {
  font-family: sans-serif;
  font-size: small;
  font-style: normal;
  -webkit-transition: opacity 0.5s;
}

.LinkContainer a {
  border: 0;
}

.LinkContainer a:after {
  content: " | ";
  color: black;
}

.LinkContainer a:last-of-type:after {
  content: "";
}

.help {
 color: gray;
 font-style: italic;
}

#message {
  font-size: small;
  font-family: sans-serif;
}

.commentStatus {
  font-style: italic;
}

.comment, .previousComment, .frozenComment {
  background-color: #ffd;
}

.overallComments {
  -webkit-box-flex: 1;
  -moz-box-flex: 1;
  margin-right: 3px;
}

.previousComment, .frozenComment {
  border: inset 1px;
  padding: 5px;
  white-space: pre-wrap;
}

.comment button {
  width: 6em;
}

div:focus {
  outline: 1px solid blue;
  outline-offset: -1px;
}

.statusBubble {
  /* The width/height get set to the bubble contents via postMessage on browsers that support it. */
  width: 450px;
  height: 20px;
  margin: 2px 2px 0 0;
  border: none;
  vertical-align: middle;
}

.pseudo_resize_event_iframe {
  height: 10%;
  width: 10%;
  position: absolute;
  top: -11%;
}

.revision {
  display: none;
}

.autosave-state {
  position: absolute;
  right: 0;
  top: -1.3em;
  padding: 0 3px;
  outline: 1px solid #DDD;
  color: #8FDF5F;
  font-size: small;   
  background-color: #EEE;
}

.autosave-state:empty {
  outline: 0px;
}
.autosave-state.saving {
  color: #E98080;
}

.clear_float {
    clear: both;
}
</style>
<script src="https://ajax.googleapis.com/ajax/libs/jquery/1.4.2/jquery.min.js"></script> 
<script src="code-review.js?version=40"></script>
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

            if @from then
                str += "<span class='revision'>" + @from + "</span>"
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

    class DiffBlock
        attr_accessor :parts

        def initialize(container)
            @parts = []
            container << self
        end

        def to_html
            str = "<div class='DiffBlock'>\n"
            str += @parts.collect{ |part| part.to_html }.join
            str += "<div class='clear_float'></div></div>\n"
        end
    end

    class DiffBlockPart
        attr_reader :className
        attr :lines

        def initialize(className, container)
            @className = className
            @lines = []
            container.parts << self
        end

        def to_html
            str = "<div class='DiffBlockPart %s'>\n" % @className
            str += @lines.collect{ |line| line.to_html }.join
            # Don't put white-space after this so adjacent inline-block DiffBlockParts will not wrap.
            str += "</div>"
        end
    end

    class DiffSection
        def initialize(lines)
            lines.length >= 1 or raise "DiffSection.parse only received %d lines" % lines.length

            matches = START_OF_SECTION_FORMAT.match(lines[0])

            if matches
                from, to = [matches[1].to_i, matches[3].to_i]
                if matches[2] and matches[4]
                    from_end = from + matches[2].to_i
                    to_end = to + matches[4].to_i
                end
            end

            @blocks = []
            diff_block = nil
            diff_block_part = nil

            for line in lines[1...lines.length]
                startOfLine = line =~ /^[-\+ ]/ ? 1 : 0
                text = line[startOfLine...line.length].chomp
                case line[0]
                when ?-
                    if (diff_block_part.nil? or diff_block_part.className != 'remove')
                        diff_block = DiffBlock.new(@blocks)
                        diff_block_part = DiffBlockPart.new('remove', diff_block)
                    end

                    diff_block_part.lines << CodeLine.new(from, nil, text)
                    from += 1 unless from.nil?
                when ?+
                    if (diff_block_part.nil? or diff_block_part.className != 'add')
                        # Put add lines that immediately follow remove lines into the same DiffBlock.
                        if (diff_block.nil? or diff_block_part.className != 'remove')
                            diff_block = DiffBlock.new(@blocks)
                        end

                        diff_block_part = DiffBlockPart.new('add', diff_block)
                    end

                    diff_block_part.lines << CodeLine.new(nil, to, text)
                    to += 1 unless to.nil?
                else
                    if (diff_block_part.nil? or diff_block_part.className != 'shared')
                        diff_block = DiffBlock.new(@blocks)
                        diff_block_part = DiffBlockPart.new('shared', diff_block)
                    end

                    diff_block_part.lines << CodeLine.new(from, to, text)
                    from += 1 unless from.nil?
                    to += 1 unless to.nil?
                end

                break if from_end and to_end and from == from_end and to == to_end
            end

            changes = [ [ [], [] ] ]
            for block in @blocks
                for block_part in block.parts
                    for line in block_part.lines
                        if (!line.fromLineNumber.nil? and !line.toLineNumber.nil?) then
                            changes << [ [], [] ]
                            next
                        end
                        changes.last.first << line if line.toLineNumber.nil?
                        changes.last.last << line if line.fromLineNumber.nil?
                    end
                end
            end

            for change in changes
                next unless change.first.length == change.last.length
                for i in (0...change.first.length)
                    from_text = change.first[i].text
                    to_text = change.last[i].text
                    next if from_text.length > MAXIMUM_INTRALINE_DIFF_LINE_LENGTH or to_text.length > MAXIMUM_INTRALINE_DIFF_LINE_LENGTH
                    raw_operations = HTMLDiff::DiffBuilder.new(from_text, to_text).operations
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

            @blocks.unshift(ContextLine.new(matches[5])) unless matches.nil? || matches[5].empty?
        end

        def to_html
            str = "<div class='DiffSection'>\n"
            str += @blocks.collect{ |block| block.to_html }.join
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
            lineClasses = ["Line", "LineContainer"]
            lineClasses << ["add"] unless @toLineNumber.nil? or !@fromLineNumber.nil?
            lineClasses << ["remove"] unless @fromLineNumber.nil? or !@toLineNumber.nil?
            lineClasses
        end

        def to_html
            markedUpText = self.text_as_html
            str = "<div class='%s'>\n" % self.classes.join(' ')
            str += "<span class='from lineNumber'>%s</span><span class='to lineNumber'>%s</span>" %
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
