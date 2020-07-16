#!/usr/bin/env python

import logging


_log = logging.getLogger(__name__)


class GenericFactory(object):

    products = None

    @classmethod
    def create(cls, description):
        return cls.products[description]()

    @classmethod
    def add(cls, description, product):
        cls.products[description] = product
