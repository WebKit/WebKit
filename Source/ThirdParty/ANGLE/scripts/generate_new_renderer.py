#!/usr/bin/python2
#
# Copyright (c) 2015 The ANGLE Project Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# generate_new_renderer.py:
#   Utility script to generate stubs for a new Renderer class.
#   Usage: generate_new_renderer.py <renderer name> <renderer suffix>
#     Renderer name is the folder for the renderer subdirectory
#     Renderer suffix is the abbreviation to append after the class names.
#
# The script is fairly robust but may not work for all new methods or
# other unexpected features. It expects that abstract methods are all
# grouped after the public destructor or after the private
# DISALLOW_COPY_AND_ASSIGN macro.

import os, sys, re, string, datetime

if len(sys.argv) < 3:
    print('Usage: ' + sys.argv[0] + ' <renderer dir name> <renderer class suffix>')
    sys.exit(1)

renderer_name = sys.argv[1]
renderer_suffix = sys.argv[2]

# change to the renderer directory
os.chdir(os.path.join(os.path.dirname(sys.argv[0]), "..", "src", "libANGLE", "renderer"))

# ensure subdir exists
if not os.path.isdir(renderer_name):
    os.mkdir(renderer_name)

impl_classes = [
    'Buffer',
    'Compiler',
    'Context',
    'Device',
    'Display',
    'FenceNV',
    'FenceSync',
    'Framebuffer',
    'Image',
    'Path',
    'Program',
    'Query',
    'Renderbuffer',
    'Sampler',
    'Shader',
    'Surface',
    'Texture',
    'TransformFeedback',
    'VertexArray',
]

h_file_template = """//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// $TypedImpl.h:
//    Defines the class interface for $TypedImpl, implementing $BaseImpl.
//

#ifndef LIBANGLE_RENDERER_${RendererNameCaps}_${TypedImplCaps}_H_
#define LIBANGLE_RENDERER_${RendererNameCaps}_${TypedImplCaps}_H_

#include "libANGLE/renderer/$BaseImpl.h"

namespace rx
{

class $TypedImpl : public $BaseImpl
{
  public:
    $TypedImpl($ConstructorParams);
    ~$TypedImpl() override;
$ImplMethodDeclarations$PrivateImplMethodDeclarations};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_${RendererNameCaps}_${TypedImplCaps}_H_
"""

cpp_file_template = """//
// Copyright $Year The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// $TypedImpl.cpp:
//    Implements the class methods for $TypedImpl.
//

#include "libANGLE/renderer/$RendererName/$TypedImpl.h"

#include "common/debug.h"

namespace rx
{

$TypedImpl::$TypedImpl($ConstructorParams) : $BaseImpl($BaseContructorArgs)
{
}

$TypedImpl::~$TypedImpl()
{
}
$ImplMethodDefinitions
}  // namespace rx
"""

def generate_impl_declaration(impl_stub):
    # ensure the wrapped lines are aligned vertically
    temp = re.sub(r'\n        ', '\n', impl_stub)
    return temp + ' override;\n'

def generate_impl_definition(impl_stub, typed_impl):
    function_signature = impl_stub.strip()

    # strip comments
    function_signature = re.sub(r'\/\/[^\n]*\n', '', function_signature).strip()

    prog = re.compile(r'^(.+[ \*\&])([^ \(\*\&]+\()')
    return_value = prog.match(function_signature).group(1)

    # ensure the wrapped lines are aligned vertically
    spaces = ' ' * len(typed_impl)
    function_signature = re.sub(r'\n          ', '\n' + spaces, function_signature)

    # add class scoping
    function_signature = prog.sub(r'\1' + typed_impl + r'::\2', function_signature)
    function_signature += '\n'

    return_statement = ''
    return_type = return_value.strip()

    if return_type != 'void':
        # specialized return values for Errors, pointers, etc
        if return_type == 'gl::Error':
            return_statement = '    return gl::InvalidOperation();\n'
        elif return_type == 'egl::Error':
            return_statement = '    return egl::EglBadAccess();\n'
        elif return_type == 'LinkResult':
            return_statement = '    return gl::InvalidOperation();\n'
        elif re.search(r'\*$', return_type):
            return_statement = '    return static_cast<' + return_type + '>(0);\n'
        elif re.search(r'const ([^ \&]+) \&$', return_type):
            obj_type = re.search(r'const ([^ \&]+) \&$', return_type).group(1)
            return_statement = '    static ' + obj_type + ' local;\n' + '    return local;\n'
        else:
            return_statement = '    return ' + return_type + '();\n'

    body = '{\n' + '    UNIMPLEMENTED();\n' + return_statement +'}\n'

    return '\n' + function_signature + body

def get_constructor_args(constructor):
    params = re.search(r'\((.*)\)', constructor).group(1)
    args = ', '.join(re.findall(r'[^\w]?(\w+)(?:\,|$)', params))
    return params, args

def parse_impl_header(base_impl):
    impl_h_file_path = base_impl + '.h'
    impl_h_file = open(impl_h_file_path, 'r')

    # extract impl stubs
    copy = False
    copy_private = False
    impl_stubs = ''
    private_impl_stubs = ''
    constructor = base_impl + '() {}'
    for line in impl_h_file:
        clean_line = line.strip()

        match = re.search(r'^(?:explicit )?(' + base_impl + r'\([^\)]*\))', clean_line);
        if match:
            constructor = match.group(1)

        # begin capture when reading the destructor.
        # begin capture also in the private scope (a few special cases)
        # end capture when we reach a non-virtual function, or different scope.
        if '~' + base_impl in clean_line:
            copy = True
            copy_private = False
        elif 'private:' in clean_line:
            copy = False
            copy_private = True
        elif ';' in clean_line and ' = 0' not in clean_line:
            copy = False
            copy_private = False
        elif '}' in clean_line or 'protected:' in clean_line or 'private:' in clean_line:
            copy = False
            copy_private = False
        elif copy:
            impl_stubs += line
        elif copy_private:
            private_impl_stubs += line

    impl_h_file.close()

    return impl_stubs, private_impl_stubs, constructor

def get_base_class(base_impl):
    impl_h_file_path = base_impl + '.h'
    with open(impl_h_file_path, 'r') as impl_h_file:
        for line in impl_h_file:
            match = re.search(r'^class ' + base_impl + r' : public (\w+)', line)
            if match:
                return match.group(1)
    return False

for impl_class in impl_classes:

    base_impl = impl_class + 'Impl'
    typed_impl = impl_class + renderer_suffix

    h_file_path = os.path.join(renderer_name, typed_impl + '.h')
    cpp_file_path = os.path.join(renderer_name, typed_impl + '.cpp')

    h_file = open(h_file_path, 'w')
    cpp_file = open(cpp_file_path, 'w')

    # extract impl stubs
    impl_stubs, private_impl_stubs, constructor = parse_impl_header(base_impl)

    # Handle base classes, skipping angle::NonCopyable.
    base_class = get_base_class(base_impl)
    if base_class and base_class != 'angle':
        base_impl_stubs, base_private_impl_stubs, base_constructor = parse_impl_header(base_class)
        impl_stubs += base_impl_stubs
        private_impl_stubs += base_private_impl_stubs

    impl_method_declarations = ''
    impl_method_definitions = ''
    private_impl_method_declarations = ''

    for impl_stub in impl_stubs.split(' = 0;\n'):
        # use 'virtual' to identify the strings with functions
        if 'virtual' in impl_stub:
            temp = re.sub(r'virtual ', '', impl_stub)
            impl_method_declarations += generate_impl_declaration(temp)
            impl_method_definitions += generate_impl_definition(temp, typed_impl)

    for impl_stub in private_impl_stubs.split(' = 0;\n'):
        # use 'virtual' to identify the strings with functions
        if 'virtual' in impl_stub:
            temp = re.sub(r'virtual ', '', impl_stub)
            private_impl_method_declarations += generate_impl_declaration(temp)
            impl_method_definitions += generate_impl_definition(temp, typed_impl)

    constructor_params, base_constructor_args = get_constructor_args(constructor)

    if private_impl_method_declarations:
        private_impl_method_declarations = "\n  private:\n" + private_impl_method_declarations

    substitutions = {
        'BaseImpl': base_impl,
        'TypedImpl': typed_impl,
        'TypedImplCaps': typed_impl.upper(),
        'RendererName': renderer_name,
        'RendererNameCaps': renderer_name.upper(),
        'ImplMethodDeclarations': impl_method_declarations,
        'ImplMethodDefinitions': impl_method_definitions,
        'ConstructorParams': constructor_params,
        'BaseContructorArgs': base_constructor_args,
        'PrivateImplMethodDeclarations': private_impl_method_declarations,
        'Year': datetime.datetime.now().year,
    }

    h_file.write(string.Template(h_file_template).substitute(substitutions))
    cpp_file.write(string.Template(cpp_file_template).substitute(substitutions))

    h_file.close()
    cpp_file.close()

# Print a block of source files to add to the build
print("Generated files:")
for impl_class in impl_classes:
    path = "libANGLE/renderer/" + renderer_name + "/" + impl_class + renderer_suffix
    print('\'' + path + ".cpp\',")
    print('\'' + path + ".h\',")
