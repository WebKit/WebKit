#!/usr/bin/env python
# -*- coding: UTF-8 -*-
"""Planet cache tool.

"""

__authors__ = [ "Scott James Remnant <scott@netsplit.com>",
                "Jeff Waugh <jdub@perkypants.org>" ]
__license__ = "Python"


import os
import sys
import time
import dbhash
import ConfigParser

import planet


def usage():
    print "Usage: planet-cache [options] CACHEFILE [ITEMID]..."
    print
    print "Examine and modify information in the Planet cache."
    print
    print "Channel Commands:"
    print " -C, --channel     Display known information on the channel"
    print " -L, --list        List items in the channel"
    print " -K, --keys        List all keys found in channel items"
    print
    print "Item Commands (need ITEMID):"
    print " -I, --item        Display known information about the item(s)"
    print " -H, --hide        Mark the item(s) as hidden"
    print " -U, --unhide      Mark the item(s) as not hidden"
    print
    print "Other Options:"
    print " -h, --help        Display this help message and exit"
    sys.exit(0)

def usage_error(msg, *args):
    print >>sys.stderr, msg, " ".join(args)
    print >>sys.stderr, "Perhaps you need --help ?"
    sys.exit(1)

def print_keys(item, title):
    keys = item.keys()
    keys.sort()
    key_len = max([ len(k) for k in keys ])

    print title + ":"
    for key in keys:
        if item.key_type(key) == item.DATE:
            value = time.strftime(planet.TIMEFMT_ISO, item[key])
        else:
            value = str(item[key])
        print "    %-*s  %s" % (key_len, key, fit_str(value, 74 - key_len))

def fit_str(string, length):
    if len(string) <= length:
        return string
    else:
        return string[:length-4] + " ..."


if __name__ == "__main__":
    cache_file = None
    want_ids = 0
    ids = []

    command = None

    for arg in sys.argv[1:]:
        if arg == "-h" or arg == "--help":
            usage()
        elif arg == "-C" or arg == "--channel":
            if command is not None:
                usage_error("Only one command option may be supplied")
            command = "channel"
        elif arg == "-L" or arg == "--list":
            if command is not None:
                usage_error("Only one command option may be supplied")
            command = "list"
        elif arg == "-K" or arg == "--keys":
            if command is not None:
                usage_error("Only one command option may be supplied")
            command = "keys"
        elif arg == "-I" or arg == "--item":
            if command is not None:
                usage_error("Only one command option may be supplied")
            command = "item"
            want_ids = 1
        elif arg == "-H" or arg == "--hide":
            if command is not None:
                usage_error("Only one command option may be supplied")
            command = "hide"
            want_ids = 1
        elif arg == "-U" or arg == "--unhide":
            if command is not None:
                usage_error("Only one command option may be supplied")
            command = "unhide"
            want_ids = 1
        elif arg.startswith("-"):
            usage_error("Unknown option:", arg)
        else:
            if cache_file is None:
                cache_file = arg
            elif want_ids:
                ids.append(arg)
            else:
                usage_error("Unexpected extra argument:", arg)

    if cache_file is None:
        usage_error("Missing expected cache filename")
    elif want_ids and not len(ids):
        usage_error("Missing expected entry ids")

    # Open the cache file directly to get the URL it represents
    try:
        db = dbhash.open(cache_file)
        url = db["url"]
        db.close()
    except dbhash.bsddb._db.DBError, e:
        print >>sys.stderr, cache_file + ":", e.args[1]
        sys.exit(1)
    except KeyError:
        print >>sys.stderr, cache_file + ": Probably not a cache file"
        sys.exit(1)

    # Now do it the right way :-)
    my_planet = planet.Planet(ConfigParser.ConfigParser())
    my_planet.cache_directory = os.path.dirname(cache_file)
    channel = planet.Channel(my_planet, url)

    for item_id in ids:
        if not channel.has_item(item_id):
            print >>sys.stderr, item_id + ": Not in channel"
            sys.exit(1)

    # Do the user's bidding
    if command == "channel":
        print_keys(channel, "Channel Keys")

    elif command == "item":
        for item_id in ids:
            item = channel.get_item(item_id)
            print_keys(item, "Item Keys for %s" % item_id)

    elif command == "list":
        print "Items in Channel:"
        for item in channel.items(hidden=1, sorted=1):
            print "    " + item.id
            print "         " + time.strftime(planet.TIMEFMT_ISO, item.date)
            if hasattr(item, "title"):
                print "         " + fit_str(item.title, 70)
            if hasattr(item, "hidden"):
                print "         (hidden)"

    elif command == "keys":
        keys = {}
        for item in channel.items():
            for key in item.keys():
                keys[key] = 1

        keys = keys.keys()
        keys.sort()

        print "Keys used in Channel:"
        for key in keys:
            print "    " + key
        print

        print "Use --item to output values of particular items."

    elif command == "hide":
        for item_id in ids:
            item = channel.get_item(item_id)
            if hasattr(item, "hidden"):
                print item_id + ": Already hidden."
            else:
                item.hidden = "yes"

        channel.cache_write()
        print "Done."

    elif command == "unhide":
        for item_id in ids:
            item = channel.get_item(item_id)
            if hasattr(item, "hidden"):
                del(item.hidden)
            else:
                print item_id + ": Not hidden."

        channel.cache_write()
        print "Done."
