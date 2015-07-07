#!/usr/bin/env python
# -*- coding: UTF-8 -*-
"""Planet aggregator library.

This package is a library for developing web sites or software that
aggregate RSS, CDF and Atom feeds taken from elsewhere into a single,
combined feed.
"""

__version__ = "2.0"
__authors__ = [ "Scott James Remnant <scott@netsplit.com>",
                "Jeff Waugh <jdub@perkypants.org>" ]
__license__ = "Python"


# Modules available without separate import
import cache
import feedparser
import sanitize
import htmltmpl
import sgmllib
try:
    import logging
except:
    import compat_logging as logging

# Limit the effect of "from planet import *"
__all__ = ("cache", "feedparser", "htmltmpl", "logging",
           "Planet", "Channel", "NewsItem")


import os
import md5
import time
import dbhash
import re

try: 
    from xml.sax.saxutils import escape
except:
    def escape(data):
        return data.replace("&","&amp;").replace(">","&gt;").replace("<","&lt;")

# Version information (for generator headers)
VERSION = ("Planet/%s +http://www.planetplanet.org" % __version__)

# Default User-Agent header to send when retreiving feeds
USER_AGENT = VERSION + " " + feedparser.USER_AGENT

# Default cache directory
CACHE_DIRECTORY = "cache"

# Default number of items to display from a new feed
NEW_FEED_ITEMS = 10

# Useful common date/time formats
TIMEFMT_ISO = "%Y-%m-%dT%H:%M:%S+00:00"
TIMEFMT_822 = "%a, %d %b %Y %H:%M:%S +0000"


# Log instance to use here
log = logging.getLogger("planet")
try:
    log.warning
except:
    log.warning = log.warn

# Defaults for the template file config sections
ENCODING        = "utf-8"
ITEMS_PER_PAGE  = 60
DAYS_PER_PAGE   = 0
OUTPUT_DIR      = "output"
DATE_FORMAT     = "%B %d, %Y %I:%M %p"
NEW_DATE_FORMAT = "%B %d, %Y"
ACTIVITY_THRESHOLD = 0

class stripHtml(sgmllib.SGMLParser):
    "remove all tags from the data"
    def __init__(self, data):
        sgmllib.SGMLParser.__init__(self)
        self.result=''
        self.feed(data)
        self.close()
    def handle_data(self, data):
        if data: self.result+=data

def template_info(item, date_format):
    """Produce a dictionary of template information."""
    info = {}
    for key in item.keys():
        if item.key_type(key) == item.DATE:
            date = item.get_as_date(key)
            info[key] = time.strftime(date_format, date)
            info[key + "_iso"] = time.strftime(TIMEFMT_ISO, date)
            info[key + "_822"] = time.strftime(TIMEFMT_822, date)
        else:
            info[key] = item[key]
    if 'title' in item.keys():
        info['title_plain'] = stripHtml(info['title']).result

    return info


class Planet:
    """A set of channels.

    This class represents a set of channels for which the items will
    be aggregated together into one combined feed.

    Properties:
        user_agent      User-Agent header to fetch feeds with.
        cache_directory Directory to store cached channels in.
        new_feed_items  Number of items to display from a new feed.
        filter          A regular expression that articles must match.
        exclude         A regular expression that articles must not match.
    """
    def __init__(self, config):
        self.config = config

        self._channels = []

        self.user_agent = USER_AGENT
        self.cache_directory = CACHE_DIRECTORY
        self.new_feed_items = NEW_FEED_ITEMS
        self.filter = None
        self.exclude = None

    def tmpl_config_get(self, template, option, default=None, raw=0, vars=None):
        """Get a template value from the configuration, with a default."""
        if self.config.has_option(template, option):
            return self.config.get(template, option, raw=raw, vars=None)
        elif self.config.has_option("Planet", option):
            return self.config.get("Planet", option, raw=raw, vars=None)
        else:
            return default

    def gather_channel_info(self, template_file="Planet"):
        date_format = self.tmpl_config_get(template_file,
                                      "date_format", DATE_FORMAT, raw=1)

        activity_threshold = int(self.tmpl_config_get(template_file,
                                            "activity_threshold",
                                            ACTIVITY_THRESHOLD))

        if activity_threshold:
            activity_horizon = \
                time.gmtime(time.time()-86400*activity_threshold)
        else:
            activity_horizon = 0

        channels = {}
        channels_list = []
        for channel in self.channels(hidden=1):
            channels[channel] = template_info(channel, date_format)
            channels_list.append(channels[channel])

            # identify inactive feeds
            if activity_horizon:
                latest = channel.items(sorted=1)
                if len(latest)==0 or latest[0].date < activity_horizon:
                    channels[channel]["message"] = \
                        "no activity in %d days" % activity_threshold

            # report channel level errors
            if not channel.url_status: continue
            status = int(channel.url_status)
            if status == 403:
               channels[channel]["message"] = "403: forbidden"
            elif status == 404:
               channels[channel]["message"] = "404: not found"
            elif status == 408:
               channels[channel]["message"] = "408: request timeout"
            elif status == 410:
               channels[channel]["message"] = "410: gone"
            elif status == 500:
               channels[channel]["message"] = "internal server error"
            elif status >= 400:
               channels[channel]["message"] = "http status %s" % status

        return channels, channels_list

    def gather_items_info(self, channels, template_file="Planet", channel_list=None):
        items_list = []
        prev_date = []
        prev_channel = None

        date_format = self.tmpl_config_get(template_file,
                                      "date_format", DATE_FORMAT, raw=1)
        items_per_page = int(self.tmpl_config_get(template_file,
                                      "items_per_page", ITEMS_PER_PAGE))
        days_per_page = int(self.tmpl_config_get(template_file,
                                      "days_per_page", DAYS_PER_PAGE))
        new_date_format = self.tmpl_config_get(template_file,
                                      "new_date_format", NEW_DATE_FORMAT, raw=1)

        for newsitem in self.items(max_items=items_per_page,
                                   max_days=days_per_page,
                                   channels=channel_list):
            item_info = template_info(newsitem, date_format)
            chan_info = channels[newsitem._channel]
            for k, v in chan_info.items():
                item_info["channel_" + k] = v
    
            # Check for the start of a new day
            if prev_date[:3] != newsitem.date[:3]:
                prev_date = newsitem.date
                item_info["new_date"] = time.strftime(new_date_format,
                                                      newsitem.date)
    
            # Check for the start of a new channel
            if item_info.has_key("new_date") \
                   or prev_channel != newsitem._channel:
                prev_channel = newsitem._channel
                item_info["new_channel"] = newsitem._channel.url
    
            items_list.append(item_info)

        return items_list

    def run(self, planet_name, planet_link, template_files, offline = False):
        log = logging.getLogger("planet.runner")

        # Create a planet
        log.info("Loading cached data")
        if self.config.has_option("Planet", "cache_directory"):
            self.cache_directory = self.config.get("Planet", "cache_directory")
        if self.config.has_option("Planet", "new_feed_items"):
            self.new_feed_items  = int(self.config.get("Planet", "new_feed_items"))
        self.user_agent = "%s +%s %s" % (planet_name, planet_link,
                                              self.user_agent)
        if self.config.has_option("Planet", "filter"):
            self.filter = self.config.get("Planet", "filter")

        # The other configuration blocks are channels to subscribe to
        for feed_url in self.config.sections():
            if feed_url == "Planet" or feed_url in template_files:
                continue

            # Create a channel, configure it and subscribe it
            channel = Channel(self, feed_url)
            self.subscribe(channel)

            # Update it
            try:
                if not offline and not channel.url_status == '410':
                    channel.update()
            except KeyboardInterrupt:
                raise
            except:
                log.exception("Update of <%s> failed", feed_url)

    def generate_all_files(self, template_files, planet_name,
                planet_link, planet_feed, owner_name, owner_email):
        
        log = logging.getLogger("planet.runner")
        # Go-go-gadget-template
        for template_file in template_files:
            manager = htmltmpl.TemplateManager()
            log.info("Processing template %s", template_file)
            try:
                template = manager.prepare(template_file)
            except htmltmpl.TemplateError:
                template = manager.prepare(os.path.basename(template_file))
            # Read the configuration
            output_dir = self.tmpl_config_get(template_file,
                                         "output_dir", OUTPUT_DIR)
            date_format = self.tmpl_config_get(template_file,
                                          "date_format", DATE_FORMAT, raw=1)
            encoding = self.tmpl_config_get(template_file, "encoding", ENCODING)
        
            # We treat each template individually
            base = os.path.splitext(os.path.basename(template_file))[0]
            url = os.path.join(planet_link, base)
            output_file = os.path.join(output_dir, base)

            # Gather information
            channels, channels_list = self.gather_channel_info(template_file) 
            items_list = self.gather_items_info(channels, template_file) 

            # Gather item information
    
            # Process the template
            tp = htmltmpl.TemplateProcessor(html_escape=0)
            tp.set("Items", items_list)
            tp.set("Channels", channels_list)
        
            # Generic information
            tp.set("generator",   VERSION)
            tp.set("name",        planet_name)
            tp.set("link",        planet_link)
            tp.set("owner_name",  owner_name)
            tp.set("owner_email", owner_email)
            tp.set("url",         url)
        
            if planet_feed:
                tp.set("feed", planet_feed)
                tp.set("feedtype", planet_feed.find('rss')>=0 and 'rss' or 'atom')
            
            # Update time
            date = time.gmtime()
            tp.set("date",        time.strftime(date_format, date))
            tp.set("date_iso",    time.strftime(TIMEFMT_ISO, date))
            tp.set("date_822",    time.strftime(TIMEFMT_822, date))

            try:
                log.info("Writing %s", output_file)
                output_fd = open(output_file, "w")
                if encoding.lower() in ("utf-8", "utf8"):
                    # UTF-8 output is the default because we use that internally
                    output_fd.write(tp.process(template))
                elif encoding.lower() in ("xml", "html", "sgml"):
                    # Magic for Python 2.3 users
                    output = tp.process(template).decode("utf-8")
                    output_fd.write(output.encode("ascii", "xmlcharrefreplace"))
                else:
                    # Must be a "known" encoding
                    output = tp.process(template).decode("utf-8")
                    output_fd.write(output.encode(encoding, "replace"))
                output_fd.close()
            except KeyboardInterrupt:
                raise
            except:
                log.exception("Write of %s failed", output_file)

    def channels(self, hidden=0, sorted=1):
        """Return the list of channels."""
        channels = []
        for channel in self._channels:
            if hidden or not channel.has_key("hidden"):
                channels.append((channel.name, channel))

        if sorted:
            channels.sort()

        return [ c[-1] for c in channels ]

    def find_by_basename(self, basename):
        for channel in self._channels:
            if basename == channel.cache_basename(): return channel

    def subscribe(self, channel):
        """Subscribe the planet to the channel."""
        self._channels.append(channel)

    def unsubscribe(self, channel):
        """Unsubscribe the planet from the channel."""
        self._channels.remove(channel)

    def items(self, hidden=0, sorted=1, max_items=0, max_days=0, channels=None):
        """Return an optionally filtered list of items in the channel.

        The filters are applied in the following order:

        If hidden is true then items in hidden channels and hidden items
        will be returned.

        If sorted is true then the item list will be sorted with the newest
        first.

        If max_items is non-zero then this number of items, at most, will
        be returned.

        If max_days is non-zero then any items older than the newest by
        this number of days won't be returned.  Requires sorted=1 to work.


        The sharp-eyed will note that this looks a little strange code-wise,
        it turns out that Python gets *really* slow if we try to sort the
        actual items themselves.  Also we use mktime here, but it's ok
        because we discard the numbers and just need them to be relatively
        consistent between each other.
        """
        planet_filter_re = None
        if self.filter:
            planet_filter_re = re.compile(self.filter, re.I)
        planet_exclude_re = None
        if self.exclude:
            planet_exclude_re = re.compile(self.exclude, re.I)
            
        items = []
        seen_guids = {}
        if not channels: channels=self.channels(hidden=hidden, sorted=0)
        for channel in channels:
            for item in channel._items.values():
                if hidden or not item.has_key("hidden"):

                    channel_filter_re = None
                    if channel.filter:
                        channel_filter_re = re.compile(channel.filter,
                                                       re.I)
                    channel_exclude_re = None
                    if channel.exclude:
                        channel_exclude_re = re.compile(channel.exclude,
                                                        re.I)
                    if (planet_filter_re or planet_exclude_re \
                        or channel_filter_re or channel_exclude_re):
                        title = ""
                        if item.has_key("title"):
                            title = item.title
                        content = item.get_content("content")

                    if planet_filter_re:
                        if not (planet_filter_re.search(title) \
                                or planet_filter_re.search(content)):
                            continue

                    if planet_exclude_re:
                        if (planet_exclude_re.search(title) \
                            or planet_exclude_re.search(content)):
                            continue

                    if channel_filter_re:
                        if not (channel_filter_re.search(title) \
                                or channel_filter_re.search(content)):
                            continue

                    if channel_exclude_re:
                        if (channel_exclude_re.search(title) \
                            or channel_exclude_re.search(content)):
                            continue

                    if not seen_guids.has_key(item.id):
                        seen_guids[item.id] = 1;
                        items.append((time.mktime(item.date), item.order, item))

        # Sort the list
        if sorted:
            items.sort()
            items.reverse()

        # Apply max_items filter
        if len(items) and max_items:
            items = items[:max_items]

        # Apply max_days filter
        if len(items) and max_days:
            max_count = 0
            max_time = items[0][0] - max_days * 84600
            for item in items:
                if item[0] > max_time:
                    max_count += 1
                else:
                    items = items[:max_count]
                    break

        return [ i[-1] for i in items ]

class Channel(cache.CachedInfo):
    """A list of news items.

    This class represents a list of news items taken from the feed of
    a website or other source.

    Properties:
        url             URL of the feed.
        url_etag        E-Tag of the feed URL.
        url_modified    Last modified time of the feed URL.
        url_status      Last HTTP status of the feed URL.
        hidden          Channel should be hidden (True if exists).
        name            Name of the feed owner, or feed title.
        next_order      Next order number to be assigned to NewsItem

        updated         Correct UTC-Normalised update time of the feed.
        last_updated    Correct UTC-Normalised time the feed was last updated.

        id              An identifier the feed claims is unique (*).
        title           One-line title (*).
        link            Link to the original format feed (*).
        tagline         Short description of the feed (*).
        info            Longer description of the feed (*).

        modified        Date the feed claims to have been modified (*).

        author          Name of the author (*).
        publisher       Name of the publisher (*).
        generator       Name of the feed generator (*).
        category        Category name (*).
        copyright       Copyright information for humans to read (*).
        license         Link to the licence for the content (*).
        docs            Link to the specification of the feed format (*).
        language        Primary language (*).
        errorreportsto  E-Mail address to send error reports to (*).

        image_url       URL of an associated image (*).
        image_link      Link to go with the associated image (*).
        image_title     Alternative text of the associated image (*).
        image_width     Width of the associated image (*).
        image_height    Height of the associated image (*).

        filter          A regular expression that articles must match.
        exclude         A regular expression that articles must not match.

    Properties marked (*) will only be present if the original feed
    contained them.  Note that the optional 'modified' date field is simply
    a claim made by the item and parsed from the information given, 'updated'
    (and 'last_updated') are far more reliable sources of information.

    Some feeds may define additional properties to those above.
    """
    IGNORE_KEYS = ("links", "contributors", "textinput", "cloud", "categories",
                   "url", "href", "url_etag", "url_modified", "tags", "itunes_explicit")

    def __init__(self, planet, url):
        if not os.path.isdir(planet.cache_directory):
            os.makedirs(planet.cache_directory)
        cache_filename = cache.filename(planet.cache_directory, url)
        cache_file = dbhash.open(cache_filename, "c", 0666)

        cache.CachedInfo.__init__(self, cache_file, url, root=1)

        self._items = {}
        self._planet = planet
        self._expired = []
        self.url = url
        # retain the original URL for error reporting
        self.configured_url = url
        self.url_etag = None
        self.url_status = None
        self.url_modified = None
        self.name = None
        self.updated = None
        self.last_updated = None
        self.filter = None
        self.exclude = None
        self.next_order = "0"
        self.cache_read()
        self.cache_read_entries()

        if planet.config.has_section(url):
            for option in planet.config.options(url):
                value = planet.config.get(url, option)
                self.set_as_string(option, value, cached=0)

    def has_item(self, id_):
        """Check whether the item exists in the channel."""
        return self._items.has_key(id_)

    def get_item(self, id_):
        """Return the item from the channel."""
        return self._items[id_]

    # Special methods
    __contains__ = has_item

    def items(self, hidden=0, sorted=0):
        """Return the item list."""
        items = []
        for item in self._items.values():
            if hidden or not item.has_key("hidden"):
                items.append((time.mktime(item.date), item.order, item))

        if sorted:
            items.sort()
            items.reverse()

        return [ i[-1] for i in items ]

    def __iter__(self):
        """Iterate the sorted item list."""
        return iter(self.items(sorted=1))

    def cache_read_entries(self):
        """Read entry information from the cache."""
        keys = self._cache.keys()
        for key in keys:
            if key.find(" ") != -1: continue
            if self.has_key(key): continue

            item = NewsItem(self, key)
            self._items[key] = item

    def cache_basename(self):
        return cache.filename('',self._id)

    def cache_write(self, sync=1):
        """Write channel and item information to the cache."""
        for item in self._items.values():
            item.cache_write(sync=0)
        for item in self._expired:
            item.cache_clear(sync=0)
        cache.CachedInfo.cache_write(self, sync)

        self._expired = []

    def feed_information(self):
        """
        Returns a description string for the feed embedded in this channel.

        This will usually simply be the feed url embedded in <>, but in the
        case where the current self.url has changed from the original
        self.configured_url the string will contain both pieces of information.
        This is so that the URL in question is easier to find in logging
        output: getting an error about a URL that doesn't appear in your config
        file is annoying.
        """
        if self.url == self.configured_url:
            return "<%s>" % self.url
        else:
            return "<%s> (formerly <%s>)" % (self.url, self.configured_url)

    def update(self):
        """Download the feed to refresh the information.

        This does the actual work of pulling down the feed and if it changes
        updates the cached information about the feed and entries within it.
        """
        info = feedparser.parse(self.url,
                                etag=self.url_etag, modified=self.url_modified,
                                agent=self._planet.user_agent)
        if info.has_key("status"):
           self.url_status = str(info.status)
        elif info.has_key("entries") and len(info.entries)>0:
           self.url_status = str(200)
        elif info.bozo and info.bozo_exception.__class__.__name__=='Timeout':
           self.url_status = str(408)
        else:
           self.url_status = str(500)

        if self.url_status == '301' and \
           (info.has_key("entries") and len(info.entries)>0):
            log.warning("Feed has moved from <%s> to <%s>", self.url, info.url)
            try:
                os.link(cache.filename(self._planet.cache_directory, self.url),
                        cache.filename(self._planet.cache_directory, info.url))
            except:
                pass
            self.url = info.url
        elif self.url_status == '304':
            log.info("Feed %s unchanged", self.feed_information())
            return
        elif self.url_status == '410':
            log.info("Feed %s gone", self.feed_information())
            self.cache_write()
            return
        elif self.url_status == '408':
            log.warning("Feed %s timed out", self.feed_information())
            return
        elif int(self.url_status) >= 400:
            log.error("Error %s while updating feed %s",
                      self.url_status, self.feed_information())
            return
        else:
            log.info("Updating feed %s", self.feed_information())

        self.url_etag = info.has_key("etag") and info.etag or None
        self.url_modified = info.has_key("modified") and info.modified or None
        if self.url_etag is not None:
            log.debug("E-Tag: %s", self.url_etag)
        if self.url_modified is not None:
            log.debug("Last Modified: %s",
                      time.strftime(TIMEFMT_ISO, self.url_modified))

        self.update_info(info.feed)
        self.update_entries(info.entries)
        self.cache_write()

    def update_info(self, feed):
        """Update information from the feed.

        This reads the feed information supplied by feedparser and updates
        the cached information about the feed.  These are the various
        potentially interesting properties that you might care about.
        """
        for key in feed.keys():
            if key in self.IGNORE_KEYS or key + "_parsed" in self.IGNORE_KEYS:
                # Ignored fields
                pass
            elif feed.has_key(key + "_parsed"):
                # Ignore unparsed date fields
                pass
            elif key.endswith("_detail"):
                # retain name and  email sub-fields
                if feed[key].has_key('name') and feed[key].name:
                    self.set_as_string(key.replace("_detail","_name"), \
                        feed[key].name)
                if feed[key].has_key('email') and feed[key].email:
                    self.set_as_string(key.replace("_detail","_email"), \
                        feed[key].email)
            elif key == "items":
                # Ignore items field
                pass
            elif key.endswith("_parsed"):
                # Date fields
                if feed[key] is not None:
                    self.set_as_date(key[:-len("_parsed")], feed[key])
            elif key == "image":
                # Image field: save all the information
                if feed[key].has_key("url"):
                    self.set_as_string(key + "_url", feed[key].url)
                if feed[key].has_key("link"):
                    self.set_as_string(key + "_link", feed[key].link)
                if feed[key].has_key("title"):
                    self.set_as_string(key + "_title", feed[key].title)
                if feed[key].has_key("width"):
                    self.set_as_string(key + "_width", str(feed[key].width))
                if feed[key].has_key("height"):
                    self.set_as_string(key + "_height", str(feed[key].height))
            elif isinstance(feed[key], (str, unicode)):
                # String fields
                try:
                    detail = key + '_detail'
                    if feed.has_key(detail) and feed[detail].has_key('type'):
                        if feed[detail].type == 'text/html':
                            feed[key] = sanitize.HTML(feed[key])
                        elif feed[detail].type == 'text/plain':
                            feed[key] = escape(feed[key])
                    self.set_as_string(key, feed[key])
                except KeyboardInterrupt:
                    raise
                except:
                    log.exception("Ignored '%s' of <%s>, unknown format",
                                  key, self.url)

    def update_entries(self, entries):
        """Update entries from the feed.

        This reads the entries supplied by feedparser and updates the
        cached information about them.  It's at this point we update
        the 'updated' timestamp and keep the old one in 'last_updated',
        these provide boundaries for acceptable entry times.

        If this is the first time a feed has been updated then most of the
        items will be marked as hidden, according to Planet.new_feed_items.

        If the feed does not contain items which, according to the sort order,
        should be there; those items are assumed to have been expired from
        the feed or replaced and are removed from the cache.
        """
        if not len(entries):
            return

        self.last_updated = self.updated
        self.updated = time.gmtime()

        new_items = []
        feed_items = []
        for entry in entries:
            # Try really hard to find some kind of unique identifier
            if entry.has_key("id"):
                entry_id = cache.utf8(entry.id)
            elif entry.has_key("link"):
                entry_id = cache.utf8(entry.link)
            elif entry.has_key("title"):
                entry_id = (self.url + "/"
                            + md5.new(cache.utf8(entry.title)).hexdigest())
            elif entry.has_key("summary"):
                entry_id = (self.url + "/"
                            + md5.new(cache.utf8(entry.summary)).hexdigest())
            else:
                log.error("Unable to find or generate id, entry ignored")
                continue

            # Create the item if necessary and update
            if self.has_item(entry_id):
                item = self._items[entry_id]
            else:
                item = NewsItem(self, entry_id)
                self._items[entry_id] = item
                new_items.append(item)
            item.update(entry)
            feed_items.append(entry_id)

            # Hide excess items the first time through
            if self.last_updated is None  and self._planet.new_feed_items \
                   and len(feed_items) > self._planet.new_feed_items:
                item.hidden = "yes"
                log.debug("Marked <%s> as hidden (new feed)", entry_id)

        # Assign order numbers in reverse
        new_items.reverse()
        for item in new_items:
            item.order = self.next_order = str(int(self.next_order) + 1)

        # Check for expired or replaced items
        feed_count = len(feed_items)
        log.debug("Items in Feed: %d", feed_count)
        for item in self.items(sorted=1):
            if feed_count < 1:
                break
            elif item.id in feed_items:
                feed_count -= 1
            elif item._channel.url_status != '226':
                del(self._items[item.id])
                self._expired.append(item)
                log.debug("Removed expired or replaced item <%s>", item.id)

    def get_name(self, key):
        """Return the key containing the name."""
        for key in ("name", "title"):
            if self.has_key(key) and self.key_type(key) != self.NULL:
                return self.get_as_string(key)

        return ""

class NewsItem(cache.CachedInfo):
    """An item of news.

    This class represents a single item of news on a channel.  They're
    created by members of the Channel class and accessible through it.

    Properties:
        id              Channel-unique identifier for this item.
        id_hash         Relatively short, printable cryptographic hash of id
        date            Corrected UTC-Normalised update time, for sorting.
        order           Order in which items on the same date can be sorted.
        hidden          Item should be hidden (True if exists).

        title           One-line title (*).
        link            Link to the original format text (*).
        summary         Short first-page summary (*).
        content         Full HTML content.

        modified        Date the item claims to have been modified (*).
        issued          Date the item claims to have been issued (*).
        created         Date the item claims to have been created (*).
        expired         Date the item claims to expire (*).

        author          Name of the author (*).
        publisher       Name of the publisher (*).
        category        Category name (*).
        comments        Link to a page to enter comments (*).
        license         Link to the licence for the content (*).
        source_name     Name of the original source of this item (*).
        source_link     Link to the original source of this item (*).

    Properties marked (*) will only be present if the original feed
    contained them.  Note that the various optional date fields are
    simply claims made by the item and parsed from the information
    given, 'date' is a far more reliable source of information.

    Some feeds may define additional properties to those above.
    """
    IGNORE_KEYS = ("categories", "contributors", "enclosures", "links",
                   "guidislink", "date", "tags")

    def __init__(self, channel, id_):
        cache.CachedInfo.__init__(self, channel._cache, id_)

        self._channel = channel
        self.id = id_
        self.id_hash = md5.new(id_).hexdigest()
        self.date = None
        self.order = None
        self.content = None
        self.cache_read()

    def update(self, entry):
        """Update the item from the feedparser entry given."""
        for key in entry.keys():
            if key in self.IGNORE_KEYS or key + "_parsed" in self.IGNORE_KEYS:
                # Ignored fields
                pass
            elif entry.has_key(key + "_parsed"):
                # Ignore unparsed date fields
                pass
            elif key.endswith("_detail"):
                # retain name, email, and language sub-fields
                if entry[key].has_key('name') and entry[key].name:
                    self.set_as_string(key.replace("_detail","_name"), \
                        entry[key].name)
                if entry[key].has_key('email') and entry[key].email:
                    self.set_as_string(key.replace("_detail","_email"), \
                        entry[key].email)
                if entry[key].has_key('language') and entry[key].language and \
                   (not self._channel.has_key('language') or \
                   entry[key].language != self._channel.language):
                    self.set_as_string(key.replace("_detail","_language"), \
                        entry[key].language)
            elif key.endswith("_parsed"):
                # Date fields
                if entry[key] is not None:
                    self.set_as_date(key[:-len("_parsed")], entry[key])
            elif key == "source":
                # Source field: save both url and value
                if entry[key].has_key("value"):
                    self.set_as_string(key + "_name", entry[key].value)
                if entry[key].has_key("url"):
                    self.set_as_string(key + "_link", entry[key].url)
            elif key == "content":
                # Content field: concatenate the values
                value = ""
                for item in entry[key]:
                    if item.type == 'text/html':
                        item.value = sanitize.HTML(item.value)
                    elif item.type == 'text/plain':
                        item.value = escape(item.value)
                    if item.has_key('language') and item.language and \
                       (not self._channel.has_key('language') or
                       item.language != self._channel.language) :
                        self.set_as_string(key + "_language", item.language)
                    value += cache.utf8(item.value)
                self.set_as_string(key, value)
            elif isinstance(entry[key], (str, unicode)):
                # String fields
                try:
                    detail = key + '_detail'
                    if entry.has_key(detail):
                        if entry[detail].has_key('type'):
                            if entry[detail].type == 'text/html':
                                entry[key] = sanitize.HTML(entry[key])
                            elif entry[detail].type == 'text/plain':
                                entry[key] = escape(entry[key])
                    self.set_as_string(key, entry[key])
                except KeyboardInterrupt:
                    raise
                except:
                    log.exception("Ignored '%s' of <%s>, unknown format",
                                  key, self.id)

        # Generate the date field if we need to
        self.get_date("date")

    def get_date(self, key):
        """Get (or update) the date key.

        We check whether the date the entry claims to have been changed is
        since we last updated this feed and when we pulled the feed off the
        site.

        If it is then it's probably not bogus, and we'll sort accordingly.

        If it isn't then we bound it appropriately, this ensures that
        entries appear in posting sequence but don't overlap entries
        added in previous updates and don't creep into the next one.
        """

        for other_key in ("updated", "modified", "published", "issued", "created"):
            if self.has_key(other_key):
                date = self.get_as_date(other_key)
                break
        else:
            date = None

        if date is not None:
            if date > self._channel.updated:
                date = self._channel.updated
#            elif date < self._channel.last_updated:
#                date = self._channel.updated
        elif self.has_key(key) and self.key_type(key) != self.NULL:
            return self.get_as_date(key)
        else:
            date = self._channel.updated

        self.set_as_date(key, date)
        return date

    def get_content(self, key):
        """Return the key containing the content."""
        for key in ("content", "tagline", "summary"):
            if self.has_key(key) and self.key_type(key) != self.NULL:
                return self.get_as_string(key)

        return ""
