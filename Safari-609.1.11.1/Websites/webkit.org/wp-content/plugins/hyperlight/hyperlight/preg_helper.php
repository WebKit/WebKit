<?php

/**
 * Copyright 2008 Konrad Rudolph
 * All rights reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/**
 * Helper functions for the Perl-compatible regular expressions.
 * @package preg_helper
 */

/**
 * Merges several regular expressions into one, using the indicated 'glue'.
 *
 * This function takes care of individual modifiers so it's safe to use
 * <i>different</i> modifiers on the individual expressions. The order of
 * sub-matches is preserved as well. Numbered back-references are adapted to
 * the new overall sub-match count. This means that it's safe to use numbered
 * back-refences in the individual expressions!
 * If {@link $names} is given, the individual expressions are captured in
 * named sub-matches using the contents of that array as names.
 * Matching pair-delimiters (e.g. <var>"{…}"</var>) are currently
 * <b>not</b> supported.
 *
 * The function assumes that all regular expressions are well-formed.
 * Behaviour is undefined if they aren't.
 *
 * This function was created after a
 * {@link http://stackoverflow.com/questions/244959/ StackOverflow discussion}.
 * Much of it was written or thought of by “porneL” and “eyelidlessness”. Many
 * thanks to both of them.
 *
 * @param string $glue  A string to insert between the individual expressions.
 *      This should usually be either the empty string, indicating
 *      concatenation, or the pipe (<var>"|"</var>), indicating alternation.
 *      Notice that this string might have to be escaped since it is treated
 *      as a normal character in a regular expression (i.e. <var>"/"</var> will
 *      end the expression and result in an invalid output).
 * @param array $expressions    The expressions to merge. The expressions may
 *      have arbitrary different delimiters and modifiers.
 * @param array $names  Optional. This is either an empty array or an array of
 *      strings of the same length as {@link $expressions}. In that case,
 *      the strings of this array are used to create named sub-matches for the
 *      expressions.
 * @return string An string representing a regular expression equivalent to the
 *      merged expressions. Returns <var>FALSE</var> if an error occurred.
 */
function preg_merge($glue, array $expressions, array $names = array()) {
    // … then, a miracle occurs.

    // Sanity check …

    $use_names = ($names !== null and count($names) !== 0);

    if (
        $use_names and count($names) !== count($expressions) or
        !is_string($glue)
    )
        return false;

    $result = array();
    // For keeping track of the names for sub-matches.
    $names_count = 0;
    // For keeping track of *all* captures to re-adjust backreferences.
    $capture_count = 0;

    foreach ($expressions as $expression) {
        if ($use_names)
            $name = str_replace(' ', '_', $names[$names_count++]);

        // Get delimiters and modifiers:

        $stripped = preg_strip($expression);

        if ($stripped === false)
            return false;

        list($sub_expr, $modifiers) = $stripped;

        // Re-adjust backreferences:
        // TODO What about \R backreferences (\0 isn't allowed, though)?
        
        // We assume that the expression is correct and therefore don't check
        // for matching parentheses.
        
        $number_of_captures = preg_match_all('/\([^?]|\(\?[^:]/', $sub_expr, $_);

        if ($number_of_captures === false)
            return false;

        if ($number_of_captures > 0) {
            $backref_expr = '/
                (?<!\\\\)        # Not preceded by a backslash,
                ((?:\\\\\\\\)*?) # zero or more escaped backslashes,
                \\\\ (\d+)       # followed by backslash plus digits.
            /x';
            $sub_expr = preg_replace_callback(
                $backref_expr,
                create_function(
                    '$m',
                    'return $m[1] . "\\\\" . ((int)$m[2] + ' . $capture_count . ');'
                ),
                $sub_expr
            );
            $capture_count += $number_of_captures;
        }

        // Last, construct the new sub-match:
        
        $modifiers = implode('', $modifiers);
        $sub_modifiers = "(?$modifiers)";
        if ($sub_modifiers === '(?)')
            $sub_modifiers = '';

        $sub_name = $use_names ? "?<$name>" : '?:';
        $new_expr = "($sub_name$sub_modifiers$sub_expr)";
        $result[] = $new_expr;
    }

    return '/' . implode($glue, $result) . '/';
}

/**
 * Strips a regular expression string off its delimiters and modifiers.
 * Additionally, normalizes the delimiters (i.e. reformats the pattern so that
 * it could have used <var>"/"</var> as delimiter).
 *
 * @param string $expression The regular expression string to strip.
 * @return array An array whose first entry is the expression itself, the
 *      second an array of delimiters. If the argument is not a valid regular
 *      expression, returns <var>FALSE</var>.
 *
 */
function preg_strip($expression) {
    if (preg_match('/^(.)(.*)\\1([imsxeADSUXJu]*)$/s', $expression, $matches) !== 1)
        return false;

    $delim = $matches[1];
    $sub_expr = $matches[2];
    if ($delim !== '/') {
        // Replace occurrences by the escaped delimiter by its unescaped
        // version and escape new delimiter.
        $sub_expr = str_replace("\\$delim", $delim, $sub_expr);
        $sub_expr = str_replace('/', '\\/', $sub_expr);
    }
    $modifiers = $matches[3] === '' ? array() : str_split(trim($matches[3]));

    return array($sub_expr, $modifiers);
}