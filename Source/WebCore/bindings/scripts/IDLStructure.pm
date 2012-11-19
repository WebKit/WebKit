# 
# KDOM IDL parser
#
# Copyright (C) 2005 Nikolas Zimmermann <wildfox@kde.org>
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

package IDLStructure;

use strict;

use Class::Struct;

# Used to represent a parsed IDL document
struct( idlDocument => {
    module => '$',   # Module identifier
    classes => '@',  # All parsed interfaces
    fileName => '$'  # file name
});

# Used to represent 'interface' blocks
struct( domClass => {
    name => '$',      # Class identifier (without module)
    parents => '@',      # List of strings
    constants => '@',    # List of 'domConstant'
    functions => '@',    # List of 'domFunction'
    attributes => '@',    # List of 'domAttribute'    
    extendedAttributes => '$', # Extended attributes
    constructors => '@', # Constructor
    isException => '$', # Used for exception interfaces
});

# Used to represent domClass contents (name of method, signature)
struct( domFunction => {
    isStatic => '$',
    signature => '$',    # Return type/Object name/extended attributes
    parameters => '@',    # List of 'domSignature'
    raisesExceptions => '@',  # Possibly raised exceptions.
});

# Used to represent domClass contents (name of attribute, signature)
struct( domAttribute => {
    type => '$',              # Attribute type (including namespace)
    isStatic => '$',
    signature => '$',         # Attribute signature
    getterExceptions => '@',  # Possibly raised exceptions.
    setterExceptions => '@',  # Possibly raised exceptions.
});

# Used to represent a map of 'variable name' <-> 'variable type'
struct( domSignature => {
    direction => '$', # Variable direction (in or out)
    name => '$',      # Variable name
    type => '$',      # Variable type
    extendedAttributes => '$', # Extended attributes
    isNullable => '$', # Is variable type Nullable (T?)
    isVariadic => '$' # Is variable variadic (long... numbers)
});

# Used to represent string constants
struct( domConstant => {
    name => '$',      # DOM Constant identifier
    type => '$',      # Type of data
    value => '$',      # Constant value
    extendedAttributes => '$', # Extended attributes
});

1;
