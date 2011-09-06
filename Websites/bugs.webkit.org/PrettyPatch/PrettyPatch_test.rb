#!/usr/bin/ruby

require 'test/unit'
require 'open-uri'
require 'PrettyPatch'

# Note: internet connection is needed to run this test suite.

class PrettyPatch_test < Test::Unit::TestCase
    class Info
        TITLE = 0
        FILE = 1
        ADD = 2
        REMOVE = 3
        SHARED = 4
    end

    PATCHES = {
        20510 => ["Single change", 1, 1, 0, 2],
        20528 => ["No 'Index' or 'diff' in patch header", 1, 4, 3, 7],
        21151 => ["Leading '/' in the path of files", 4, 9, 1, 16],
        # Binary files use shared blocks, there are three in 30488.
        30488 => ["Quoted filenames in git diff", 23, 28, 25, 64 + 3],
        23920 => ["Mac line ending", 3, 3, 0, 5],
        39615 => ["Git signature", 2, 2, 0, 3],
        80852 => ["Changes one line plus ChangeLog", 2, 2, 1, 4],
        83127 => ["Only add stuff", 2, 2, 0, 3],
        85071 => ["Adds and removes from a file plus git signature", 2, 5, 3, 9],
        104633 => ["Delta mechanism for binary patch in git diff", 12, 3, 5, 3],
    }

    def get_patch_uri(id)
        "https://bugs.webkit.org/attachment.cgi?id=" + id.to_s
    end

    def get_patch(id)
        result = nil
        patch_uri = get_patch_uri(id)
        begin
            result = open(patch_uri) { |f| result = f.read }
        rescue => exception
            assert(false, "Fail to get patch " + patch_uri)
        end
        result
    end

    def check_one_patch(id, info)
        patch = get_patch(id)
        description = get_patch_uri(id)
        description +=  " (" + info[Info::TITLE] + ")" unless info[Info::TITLE].nil?
        puts "Testing " + description
        pretty = nil
        assert_nothing_raised("Crash while prettifying " + description) {
            pretty = PrettyPatch.prettify(patch)
        }
        assert(pretty, "Empty result while prettifying " + description)
        assert_equal(info[Info::FILE], $last_prettify_file_count, "Wrong number of files changed in " + description)
        assert_equal(info[Info::ADD], $last_prettify_part_count["add"], "Wrong number of 'add' parts in " + description)
        assert_equal(info[Info::REMOVE], $last_prettify_part_count["remove"], "Wrong number of 'remove' parts in " + description)
        assert_equal(info[Info::SHARED], $last_prettify_part_count["shared"], "Wrong number of 'shared' parts in " + description)
        assert_equal(0, $last_prettify_part_count["binary"], "Wrong number of 'binary' parts in " + description)
    end

    def test_patches
        PATCHES.each { |id, info| check_one_patch(id, info) }
    end
end
