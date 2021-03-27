#
# Copyright (C) 2011 Google Inc.  All rights reserved.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Library General Public License
# along with this library; see the file COPYING.LIB.  If not, write to
# the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
# Boston, MA 02110-1301, USA.
#


def func1():


def func2():
    return


def func3():
    # comment
    return

# def funcInsideComment():


def func4(a):
    return


def func5(a, b, c):
    return


def func6(a, b, \
          c, d):
    return


def funcOverloaded(a):
    return


def funcOverloaded(a, b):
    return


def funcOverloaded(a, b, c=100):
    return


def func7():
    pass

    def func8():
        pass

        def func9():
            pass
        pass
    pass


class Class1:
    pass


class Class2:
    pass

    class Class3:
        pass

        class Class4:
            pass
        pass
    pass


class Class5:
    pass

    def func10():
        pass
    pass

    def func11():
        pass
    pass

"""class A"""

'class A'

"class A"

"""def A"""

'def A'

"def A"

"""class Class6"""
class Class6:
    """class Class6_1"""
    def __init__(self):
        """class Class6_2"""
        pass

"""def Class7"""
class Class7:
    """def Class7_1"""
    def __init__(self):
        """def Class7_2"""
        pass

"""a
class b
"""

"""a
def b
"""

"""
class class8
"""
class Class8:
    pass

    """
    def Class8_1
    """
    def __init__(self):
        pass


MULTILINE_STRING_VARIABLE = '''
some text
some more text
'''

class Class9:
    pass
