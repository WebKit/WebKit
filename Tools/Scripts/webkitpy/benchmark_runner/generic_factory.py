import logging


_log = logging.getLogger(__name__)


class GenericFactory(object):

    products = None

    @classmethod
    def create(cls, description, *args, **kwargs):
        return cls.products[description](*args, **kwargs)

    @classmethod
    def add(cls, description, product):
        cls.products[description] = product
