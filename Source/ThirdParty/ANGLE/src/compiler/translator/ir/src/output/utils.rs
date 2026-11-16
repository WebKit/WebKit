// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Output utilities for code generators.

pub fn get_swizzle(index: u32) -> &'static str {
    match index {
        0 => "x",
        1 => "y",
        2 => "z",
        3 => "w",
        _ => unreachable!("Internal error: Unexpected swizzle index"),
    }
}

pub fn get_swizzle_multi(indices: &[u32]) -> String {
    indices.iter().map(|&index| get_swizzle(index)).collect::<Vec<_>>().join("")
}

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum Associativity {
    Left,
    None,
}

/// Expression precedence groups shared across text-based IR output generators
/// (WGSL, GLSL/ESSL, MSL, and HLSL), ordered from weakest (`Statement = 0`) to
/// strongest (`Primary = 10`).
#[derive(Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Debug)]
pub enum Precedence {
    Statement = 0,
    BitwiseOr = 1,
    BitwiseXor = 2,
    BitwiseAnd = 3,
    Relational = 4,
    Shift = 5,
    Additive = 6,
    Multiplicative = 7,
    // Equivalent to `Unary` for binary operator binding (`-x * y` needs no parentheses), but one
    // level below `Unary` so nested negations (`-(-x)`) are parenthesized (`Negate < Unary`)
    // instead of emitting `--x`.
    Negate = 8,
    Unary = 9,
    Primary = 10,
}

impl Precedence {
    /// Returns the associativity for binary operators in this precedence group per WGSL section
    /// 8.20.
    pub const fn associativity(self) -> Associativity {
        match self {
            Precedence::Additive
            | Precedence::Multiplicative
            | Precedence::BitwiseAnd
            | Precedence::BitwiseXor
            | Precedence::BitwiseOr => Associativity::Left,
            _ => Associativity::None,
        }
    }

    /// Returns the minimum precedence required for operands of binary operators in this group
    /// to be emitted without parentheses per WGSL section 8.20.
    pub const fn binding(self) -> Precedence {
        match self {
            Precedence::Relational => Precedence::Shift,
            Precedence::Additive => Precedence::Multiplicative,
            Precedence::Multiplicative
            | Precedence::Shift
            | Precedence::BitwiseAnd
            | Precedence::BitwiseXor
            | Precedence::BitwiseOr => Precedence::Negate,
            _ => Precedence::Primary,
        }
    }

    /// Returns `text` unparenthesized if `self >= min_precedence_unparenthesized`, or wrapped in
    /// parentheses `({text})` otherwise.
    ///
    /// `min_precedence_unparenthesized` is the minimum `Precedence` an inner operand must have to
    /// omit parentheses inside the outer operation:
    /// * Binary operators: `Precedence::binding()` (the group above the binary operator).
    /// * Unary operators (`-`, `!`, `~`): `Precedence::Unary`.
    /// * Postfix/primary operations (`.x`, `[]`, `.field`): `Precedence::Primary`.
    pub fn parenthesize_if_needed(
        self,
        text: &str,
        min_precedence_unparenthesized: Precedence,
    ) -> String {
        if self >= min_precedence_unparenthesized { text.to_string() } else { format!("({text})") }
    }
}

/// Stores operator information for formatting binary expressions with minimal, unambiguous
/// parentheses across all text-based IR output generators (WGSL, GLSL/ESSL, MSL, and HLSL).
///
/// In C-family shading languages — GLSL/ESSL, MSL (C++14), and HLSL — binary and unary operator
/// precedence groups form a single linear hierarchy:
/// `Primary/Postfix > Unary > Multiplicative (*, /, %) > Additive (+, -) > Shift (<<, >>) >`
/// `Relational (<, <=, >, >=) > Equality (==, !=) > BitwiseAnd (&) > BitwiseXor (^) > BitwiseOr
/// (|)`, with left-to-right associativity for all binary operators above assignment.
///
/// In WGSL (§8.20, https://www.w3.org/TR/WGSL/#operator-precedence-associativity), operator
/// precedence is strictly more conservative and forms a partial order (DAG) rather than a total
/// linear order:
/// * Both `Additive` (`+`, `-`) and `Shift` (`<<`, `>>`) bind tighter than `Relational` (`<`, `==`)
///   so `a + b < c << d` needs no parentheses, yet `Additive` and `Shift` cannot bind to each other
///   without explicit parentheses (`(a + b) << c`), and `Shift` is non-associative (`(a << b) <<
///   c`).
/// * Relational (`<`, `<=`, `>`, `>=`) and equality (`==`, `!=`) operators share a single
///   non-associative `Relational` group, requiring parentheses when nested (`(a < b) == (c < d)`).
/// * Bitwise `&`, `^`, `|` are each left-associative with themselves (`a & b & c`), but require
///   parentheses when mixed with each other (`(a & b) | c`) or with arithmetic, shift, or
///   relational operators (`(a + b) & c`, `(a & b) == c`).
///
/// Because WGSL's rules are a strict subset of the C/C++ operator hierarchy used by GLSL/ESSL,
/// MSL, and HLSL (every expression where WGSL omits parentheses has the exact same relative
/// precedence and left-to-right associativity in GLSL/ESSL, MSL, and HLSL, and every extra
/// parenthesis required by WGSL preserves evaluation order while also avoiding C++/MSL
/// `-Wparentheses` warnings on mixed shift/bitwise/relational expressions), all text backends can
/// share these rules:
/// * `precedence`: The `Precedence` group of the expression produced by this operator.
/// * `binding`: The minimum `Precedence` an operand must have to be emitted without parentheses
///   (matching the "Binding" column in WGSL §8.20). When WGSL specifies "All above", `binding` is
///   the group immediately stronger than `precedence` (e.g. `Multiplicative` for `+`/`-`, or
///   `Shift` for `<`/`==`). When WGSL requires explicit parentheses unless the operand is unary or
///   primary (e.g. `&`, `^`, `|`, `<<`, `>>`), `binding` is `Precedence::Unary`.
///
/// When formatting `l op r`:
/// * The right operand `r` always requires `r_precedence >= binding` to omit parentheses. For
///   example, when formatting `a + r` (`precedence = Additive`, `binding = Multiplicative`), `r =
///   (b * c)` (`Multiplicative >= Multiplicative`) needs no parentheses (`a + b * c`), whereas `r =
///   (b + c)` (`Additive < Multiplicative`) is weaker than `binding` and is parenthesized (`a + (b
///   + c)`).
/// * For `Associativity::Left` operators, the left operand `l` may also omit parentheses when it
///   belongs to the same group (`l_precedence == precedence`), allowing left-to-right chaining like
///   `(a + b) + c` -> `a + b + c` or `(a & b) & c` -> `a & b & c`. Any other left operand still
///   requires `l_precedence >= binding` to omit parentheses (so `(a | b) & c` keeps its parentheses
///   because `BitwiseOr != BitwiseAnd` and `BitwiseOr < Unary`).
/// * For `Associativity::None` operators (such as `<`, `==`, `<<`), chaining the operator group
///   with itself (`a < b < c` or `a << b << c`) is forbidden in WGSL and error-prone in C-family
///   backends, so both `l` and `r` require `precedence >= binding` to be emitted without
///   parentheses.
pub struct BinaryOpInfo {
    pub op_str: &'static str,
    pub precedence: Precedence,
    pub associativity: Associativity,
    pub binding: Precedence,
}

impl BinaryOpInfo {
    pub const fn new(op_str: &'static str, precedence: Precedence) -> Self {
        Self {
            op_str,
            precedence,
            associativity: precedence.associativity(),
            binding: precedence.binding(),
        }
    }

    pub fn format_expr(
        &self,
        l_text: &str,
        l_precedence: Precedence,
        r_text: &str,
        r_precedence: Precedence,
    ) -> (String, Precedence) {
        let l_str = if self.associativity == Associativity::Left && l_precedence == self.precedence
        {
            l_text.to_string()
        } else {
            l_precedence.parenthesize_if_needed(l_text, self.binding)
        };
        let r_str = r_precedence.parenthesize_if_needed(r_text, self.binding);
        (format!("{l_str} {} {r_str}", self.op_str), self.precedence)
    }
}
