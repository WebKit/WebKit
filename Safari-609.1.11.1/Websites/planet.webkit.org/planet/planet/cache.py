#!/usr/bin/env python
# -*- coding: UTF-8 -*-
"""Item cache.

Between runs of Planet we need somewhere to store the feed information
we parsed, this is so we don't lose information when a particular feed
goes away or is too short to hold enough items.

This module provides the code to handle this cache transparently enough
that the rest of the code can take the persistance for granted.
"""

import os
import re


# Regular expressions to sanitise cache filenames
re_url_scheme    = re.compile(r'^[^:]*://')
re_slash         = re.compile(r'[?/]+')
re_initial_cruft = re.compile(r'^[,.]*')
re_final_cruft   = re.compile(r'[,.]*$')


class CachedInfo:
    """Cached information.

    This class is designed to hold information that is stored in a cache
    between instances.  It can act both as a dictionary (c['foo']) and
    as an object (c.foo) to get and set values and supports both string
    and date values.

    If you wish to support special fields you can derive a class off this
    and implement get_FIELD and set_FIELD functions which will be
    automatically called.
    """
    STRING = "string"
    DATE   = "date"
    NULL   = "null"

    def __init__(self, cache, id_, root=0):
        self._type = {}
        self._value = {}
        self._cached = {}

        self._cache = cache
        self._id = id_.replace(" ", "%20")
        self._root = root

    def cache_key(self, key):
        """Return the cache key name for the given key."""
        key = key.replace(" ", "_")
        if self._root:
            return key
        else:
            return self._id + " " + key

    def cache_read(self):
        """Read information from the cache."""
        if self._root:
            keys_key = " keys"
        else:
            keys_key = self._id

        if self._cache.has_key(keys_key):
            keys = self._cache[keys_key].split(" ")
        else:
            return

        for key in keys:
            cache_key = self.cache_key(key)
            if not self._cached.has_key(key) or self._cached[key]:
                # Key either hasn't been loaded, or is one for the cache
                self._value[key] = self._cache[cache_key]
                self._type[key] = self._cache[cache_key + " type"]
                self._cached[key] = 1

    def cache_write(self, sync=1):
        """Write information to the cache."""
        self.cache_clear(sync=0)

        keys = []
        for key in self.keys():
            cache_key = self.cache_key(key)
            if not self._cached[key]:
                if self._cache.has_key(cache_key):
                    # Non-cached keys need to be cleared
                    del(self._cache[cache_key])
                    del(self._cache[cache_key + " type"])
                continue

            keys.append(key)
            self._cache[cache_key] = self._value[key]
            self._cache[cache_key + " type"] = self._type[key]

        if self._root:
            keys_key = " keys"
        else:
            keys_key = self._id

        self._cache[keys_key] = " ".join(keys)
        if sync:
            self._cache.sync()

    def cache_clear(self, sync=1):
        """Remove information from the cache."""
        if self._root:
            keys_key = " keys"
        else:
            keys_key = self._id

        if self._cache.has_key(keys_key):
            keys = self._cache[keys_key].split(" ")
            del(self._cache[keys_key])
        else:
            return

        for key in keys:
            cache_key = self.cache_key(key)
            del(self._cache[cache_key])
            del(self._cache[cache_key + " type"])

        if sync:
            self._cache.sync()

    def has_key(self, key):
        """Check whether the key exists."""
        key = key.replace(" ", "_")
        return self._value.has_key(key)

    def key_type(self, key):
        """Return the key type."""
        key = key.replace(" ", "_")
        return self._type[key]

    def set(self, key, value, cached=1):
        """Set the value of the given key.

        If a set_KEY function exists that is called otherwise the
        string function is called and the date function if that fails
        (it nearly always will).
        """
        key = key.replace(" ", "_")

        try:
            func = getattr(self, "set_" + key)
        except AttributeError:
            pass
        else:
            return func(key, value)

        if value == None:
            return self.set_as_null(key, value)
        else:
            try:
                return self.set_as_string(key, value)
            except TypeError:
                return self.set_as_date(key, value)

    def get(self, key):
        """Return the value of the given key.

        If a get_KEY function exists that is called otherwise the
        correctly typed function is called if that exists.
        """
        key = key.replace(" ", "_")

        try:
            func = getattr(self, "get_" + key)
        except AttributeError:
            pass
        else:
            return func(key)

        try:
            func = getattr(self, "get_as_" + self._type[key])
        except AttributeError:
            pass
        else:
            return func(key)

        return self._value[key]

    def set_as_string(self, key, value, cached=1):
        """Set the key to the string value.

        The value is converted to UTF-8 if it is a Unicode string, otherwise
        it's assumed to have failed decoding (feedparser tries pretty hard)
        so has all non-ASCII characters stripped.
        """
        value = utf8(value)

        key = key.replace(" ", "_")
        self._value[key] = value
        self._type[key] = self.STRING
        self._cached[key] = cached

    def get_as_string(self, key):
        """Return the key as a string value."""
        key = key.replace(" ", "_")
        if not self.has_key(key):
            raise KeyError, key

        return self._value[key]

    def set_as_date(self, key, value, cached=1):
        """Set the key to the date value.

        The date should be a 9-item tuple as returned by time.gmtime().
        """
        value = " ".join([ str(s) for s in value ])

        key = key.replace(" ", "_")
        self._value[key] = value
        self._type[key] = self.DATE
        self._cached[key] = cached

    def get_as_date(self, key):
        """Return the key as a date value."""
        key = key.replace(" ", "_")
        if not self.has_key(key):
            raise KeyError, key

        value = self._value[key]
        return tuple([ int(i) for i in value.split(" ") ])

    def set_as_null(self, key, value, cached=1):
        """Set the key to the null value.

        This only exists to make things less magic.
        """
        key = key.replace(" ", "_")
        self._value[key] = ""
        self._type[key] = self.NULL
        self._cached[key] = cached

    def get_as_null(self, key):
        """Return the key as the null value."""
        key = key.replace(" ", "_")
        if not self.has_key(key):
            raise KeyError, key

        return None

    def del_key(self, key):
        """Delete the given key."""
        key = key.replace(" ", "_")
        if not self.has_key(key):
            raise KeyError, key

        del(self._value[key])
        del(self._type[key])
        del(self._cached[key])

    def keys(self):
        """Return the list of cached keys."""
        return self._value.keys()

    def __iter__(self):
        """Iterate the cached keys."""
        return iter(self._value.keys())

    # Special methods
    __contains__ = has_key
    __setitem__  = set_as_string
    __getitem__  = get
    __delitem__  = del_key
    __delattr__  = del_key

    def __setattr__(self, key, value):
        if key.startswith("_"):
            self.__dict__[key] = value
        else:
            self.set(key, value)

    def __getattr__(self, key):
        if self.has_key(key):
            return self.get(key)
        else:
            raise AttributeError, key


def filename(directory, filename):
    """Return a filename suitable for the cache.

    Strips dangerous and common characters to create a filename we
    can use to store the cache in.
    """
    filename = re_url_scheme.sub("", filename)
    filename = re_slash.sub(",", filename)
    filename = re_initial_cruft.sub("", filename)
    filename = re_final_cruft.sub("", filename)

    return os.path.join(directory, filename)

def utf8(value):
    """Return the value as a UTF-8 string."""
    if type(value) == type(u''):
        return value.encode("utf-8")
    else:
        try:
            return unicode(value, "utf-8").encode("utf-8")
        except UnicodeError:
            try:
                return unicode(value, "iso-8859-1").encode("utf-8")
            except UnicodeError:
                return unicode(value, "ascii", "replace").encode("utf-8")
