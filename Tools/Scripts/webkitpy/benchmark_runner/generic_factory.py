#!/usr/bin/env python

import logging
import os

from utils import loadModule, ModuleNotFoundError


_log = logging.getLogger(__name__)


class GenericFactory(object):

    products = None

    @classmethod
    def iterateGetItem(cls, options, keys):
        ret = options
        for key in keys:
            try:
                ret = ret.__getitem__(key)
            except KeyError:
                raise
        return ret

    @classmethod
    def create(cls, descriptions):
        try:
            return loadModule(cls.iterateGetItem(cls.products, descriptions))()
        except:
            raise
