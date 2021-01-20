/*
 * Copyright (C) 2015 Apple Inc.  All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

// There are other form of function, but I just illustrate them in the class below.
// `freeFunction()`
func freeFunction() {
}

// May also be `struct`, `enum`, `extension`, or `protocol`.
class MyClass {

    // `MyClass.function()`
    func function() {
    }

    // By default the first argument to functions is unnamed, so in Swift you'd call this
    // as functionWithArgument(foo).
    //
    // `MyClass.functionWithArgument(_:)`
    func functionWithArgument(arg: Arg) {
    }

    // Second arguments do get a name.
    //
    // `MyClass.functionWithMoreArguments(_:arg2:)
    func functionWithMoreArguments(arg1: Arg, arg2: Arg) {
    }

    // You can give the first argument a name by specifying an explicit external name.
    // This would be called as functionWithNamedFirstArgument(argument: 1)
    //
    // `MyClass.functionWithNamedFirstArgument(argument:)`
    func functionWithNamedFirstArgument(argument arg: Arg) {
    }

    // You can also give a different external name to other arguments as so.
    //
    // `MyClass.functionWithNamedFirstAndSecondArgument(first:second:)`
    func functionWithNamedFirstAndSecondArgument(first arg1: Arg, second arg2: Arg) {
    }

    // Now for some things I don't know how to specify but can give random suggestions for…

    // I've not seen clever ways of differentiating class functions from instance functions :(
    //
    // `MyClass.classFunction()`
    class func classFunction() {
    }

    // These map to what would be -computedVariable and -setComputedVariable: in Objective-C.
    // To make things fun computed variables can also exist outside of a class definition, so
    // I think they should still be prefixed.
    var readWriteComputedVariable: Var {
        // `MyClass.readWriteComputedVariable { get }`
        get { return 0 }
        // `MyClass.readWriteComputedVariable { set }`
        set { print(newValue) }
    }

    // `MyClass.readOnlyComputedVariable { get }`
    var readOnlyComputedVariable: Var {
        return 0
    }

}

// Swift functions also support type overloading. Traditionally we don't include types in
// the ChangeLogs for Objective-C, but I assume this can come up in C++ code so I'd suggest
// doing whatever we do there. That said, overloading is only supported in pure Swift,
// which I don't anticipate needing to worry about for a while longer.