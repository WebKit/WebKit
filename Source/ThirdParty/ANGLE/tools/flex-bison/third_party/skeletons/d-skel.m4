                                                            -*- Autoconf -*-

# D skeleton dispatching for Bison.

# Copyright (C) 2018-2021 Free Software Foundation, Inc.

# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

b4_glr_if(             [b4_complain([%%glr-parser not supported for D])])
b4_nondeterministic_if([b4_complain([%%nondeterministic-parser not supported for D])])

m4_define_default([b4_used_skeleton], [b4_skeletonsdir/[lalr1.d]])
m4_define_default([b4_skeleton], ["b4_basename(b4_used_skeleton)"])

m4_include(b4_used_skeleton)
