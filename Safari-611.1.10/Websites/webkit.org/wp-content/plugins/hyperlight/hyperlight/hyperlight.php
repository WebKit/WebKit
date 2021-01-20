<?php

/*
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

/*
 * TODO list
 * =========
 *
 * - FIXME Nested syntax elements create redundant nested tags under certain
 *   circumstances. This can be reproduced by the following PHP snippet:
 *
 *      <pre class="<?php echo; ? >">
 *
 *   (Remove space between `?` and `>`).
 *   Although this no longer occurs, it is fixed by checking for `$token === ''`
 *   in the `emit*` methods. This should never happen anyway. Probably something
 *   to do with the zero-width lookahead in the PHP syntax definition.
 *
 * - `hyperlight_calculate_fold_marks`: refactor, write proper handler
 *
 * - Line numbers (on client-side?)
 *
 */

/**
 * Hyperlight source code highlighter for PHP.
 * @package hyperlight
 */

/** @ignore */
require_once('preg_helper.php');

if (!function_exists('array_peek')) {
    /**
     * @internal
     * This does exactly what you think it does. */
    function array_peek(array &$array) {
        $cnt = count($array);
        return $cnt === 0 ? null : $array[$cnt - 1];
    }
}

/**
 * @internal
 * For internal debugging purposes.
 */
function dump($obj, $descr = null) {
    if ($descr !== null)
        echo "<h3>$descr</h3>";
    ob_start();
    var_dump($obj);
    $dump = ob_get_clean();
    ?><pre><?php echo htmlspecialchars($dump); ?></pre><?php
    return true;
}

/**
 * Raised when the grammar offers a rule that has not been defined.
 */
class NoMatchingRuleException extends Exception {
    /** @internal */
    public function __construct($states, $position, $code) {
        $state = array_pop($states);
        parent::__construct(
            "State '$state' has no matching rule at position $position:\n" .
            $this->errorSurrounding($code, $position)
        );
    }

    // Try to extract the location of the error more or less precisely.
    // Only used for a comprehensive display.
    private function errorSurrounding($code, $pos) {
        $size = 10;
        $begin = $pos < $size ? 0 : $pos - $size;
        $end = $pos + $size > strlen($code) ? strlen($code) : $pos + $size;
        $offs = $pos - $begin;
        return substr($code, $begin, $end - $begin) . "\n" . sprintf("%{$offs}s", '^');
    }
}

/**
 * Represents a nesting rule in the grammar of a language definition.
 *
 * Individual rules can either be represented by raw strings ("simple" rules) or
 * by a nesting rule. Nesting rules specify where they can start and end. Inside
 * a nesting rule, other rules may be applied (both simple and nesting).
 * For example, a nesting rule may define a string literal. Inside that string,
 * other rules may be applied that recognize escape sequences.
 *
 * To use a nesting rule, supply how it may start and end, e.g.:
 * <code>
 * $string_rule = array('string' => new Rule('/"/', '/"/'));
 * </code>
 * You also need to specify nested states:
 * <code>
 * $string_states = array('string' => 'escaped');
 * <code>
 * Now you can add another rule for <var>escaped</var>:
 * <code>
 * $escaped_rule = array('escaped' => '/\\(x\d{1,4}|.)/');
 * </code>
 */
class Rule {
    /**
     * Common rules.
     */

    const ALL_WHITESPACE = '/(\s|\r|\n)+/';
    const C_IDENTIFIER = '/[a-z_][a-z0-9_]*/i';
    const C_COMMENT = '#//.*?\n|/\*.*?\*/#s';
    const C_MULTILINECOMMENT = '#/\*.*?\*/#s';
    const DOUBLEQUOTESTRING = '/"(?:\\\\"|.)*?"/s';
    const SINGLEQUOTESTRING = "/'(?:\\\\'|.)*?'/s";
    const C_DOUBLEQUOTESTRING = '/L?"(?:\\\\"|.)*?"/s';
    const C_SINGLEQUOTESTRING = "/L?'(?:\\\\'|.)*?'/s";
    const STRING = '/"(?:\\\\"|.)*?"|\'(?:\\\\\'|.)*?\'/s';
    const C_NUMBER = '/
        (?: # Integer followed by optional fractional part.
            (?:
                0(?:
                    x[0-9a-f]+
                    |
                    [0-7]*
                )
                |
                \d+
            )
            (?:\.\d*)?
            (?:e[+-]\d+)?
        )
        |
        (?: # Just the fractional part.
            (?:\.\d+)
            (?:e[+-]?\d+)?
        )
        /ix';

    private $_start;
    private $_end;

    /** @ignore */
    public function __construct($start, $end = null) {
        $this->_start = $start;
        $this->_end = $end;
    }

    /**
     * Returns the pattern with which this rule starts.
     * @return string
     */
    public function start() {
        return $this->_start;
    }

    /**
     * Returns the pattern with which this rule may end.
     * @return string
     */
    public function end() {
        return $this->_end;
    }
}

/**
 * Abstract base class of all Hyperlight language definitions.
 *
 * In order to define a new language definition, this class is inherited.
 * The only function that needs to be overridden is the constructor. Helper
 * functions from the base class can then be called to construct the grammar
 * and store additional information.
 * The name of the subclass must be of the schema <var>{Lang}Language</var>,
 * where <var>{Lang}</var> is a short, unique name for the language starting
 * with a capital letter and continuing in lower case. For example,
 * <var>PhpLanguage</var> is a valid name. The language definition must
 * reside in a file located at <var>languages/{lang}.php</var>. Here,
 * <var>{lang}</var> is the all-lowercase spelling of the name, e.g.
 * <var>languages/php.php</var>.
 *
 */
abstract class HyperLanguage {
    private $_states = array();
    private $_rules = array();
    private $_mappings = array();
    private $_info = array();
    private $_extensions = array();
    private $_caseInsensitive = false;
    private $_postProcessors = array();

    private static $_languageCache = array();
    private static $_compiledLanguageCache = array();
    private static $_filetypes;

    /**
     * Indices for information.
     */

    const NAME = 1;
    const VERSION = 2;
    const AUTHOR = 10;
    const WEBSITE = 5;
    const EMAIL = 6;

    /**
     * Retrieves a language definition name based on a file extension.
     *
     * Uses the contents of the <var>languages/filetypes</var> file to
     * guess the language definition name from a file name extension.
     * This file has to be generated using the
     * <var>collect-filetypes.php</var> script every time the language
     * definitions have been changed.
     *
     * @param string $ext the file name extension.
     * @return string The language definition name or <var>NULL</var>.
     */
    public static function nameFromExt($ext) {
        if (self::$_filetypes === null) {
            $ft_content = file('languages/filetypes', 1);

            foreach ($ft_content as $line) {
                list ($name, $extensions) = explode(':', trim($line));
                $extensions = explode(',', $extensions);
                // Inverse lookup.
                foreach ($extensions as $extension)
                    $ft_data[$extension] = $name;
            }
            self::$_filetypes = $ft_data;
        }
        $ext = strtolower($ext);
        return
            array_key_exists($ext, self::$_filetypes) ?
            self::$_filetypes[strtolower($ext)] : null;
    }

    public static function compile(HyperLanguage $lang) {
        $id = $lang->id();
        if (!isset(self::$_compiledLanguageCache[$id]))
            self::$_compiledLanguageCache[$id] = $lang->makeCompiledLanguage();
        return self::$_compiledLanguageCache[$id];
    }

    public static function compileFromName($lang) {
		$HyperLang = self::fromName($lang);
		if (false === $HyperLang) return false;
        return self::compile($HyperLang);
    }

    protected static function exists($lang) {
        return isset(self::$_languageCache[$lang]) or
               file_exists("languages/$lang.php");
    }

    protected static function fromName($lang) {
        if (!isset(self::$_languageCache[$lang])) {
			$file = realpath(dirname(__FILE__)."/languages/$lang.php");
			if (!file_exists($file)) {
                $lang = self::nameFromExt($lang);
    			$file = realpath(dirname(__FILE__)."/languages/$lang.php");
			}
			if (!file_exists($file)) return false;
 			require_once($file);
            $klass = ucfirst("{$lang}Language");
           	self::$_languageCache[$lang] = new $klass();
        }
       	return self::$_languageCache[$lang];
    }

    public function id() {
        $klass = get_class($this);
        return strtolower(substr($klass, 0, strlen($klass) - strlen('Language')));
    }

    protected function setCaseInsensitive($value) {
        $this->_caseInsensitive = $value;
    }

    protected function addStates(array $states) {
        $this->_states = self::mergeProperties($this->_states, $states);
    }

    protected function getState($key) {
        return $this->_states[$key];
    }

    protected function removeState($key) {
        unset($this->_states[$key]);
    }

    protected function addRules(array $rules) {
        $this->_rules = self::mergeProperties($this->_rules, $rules);
    }

    protected function getRule($key) {
        return $this->_rules[$key];
    }

    protected function removeRule($key) {
        unset($this->_rules[$key]);
    }

    protected function addMappings(array $mappings) {
        // TODO Implement nested mappings.
        $this->_mappings = array_merge($this->_mappings, $mappings);
    }

    protected function getMapping($key) {
        return $this->_mappings[$key];
    }

    protected function removeMapping($key) {
        unset($this->_mappings[$key]);
    }

    protected function setInfo(array $info) {
        $this->_info = $info;
    }

    protected function setExtensions(array $extensions) {
        $this->_extensions = $extensions;
    }

    protected function addPostprocessing($rule, HyperLanguage $language) {
        $this->_postProcessors[$rule] = $language;
    }

//    protected function addNestedLanguage(HyperLanguage $language, $hoistBackRules) {
//        $prefix = get_class($language);
//        if (!is_array($hoistBackRules))
//            $hoistBackRules = array($hoistBackRules);
//
//        $states = array();  // Step 1: states
//
//        foreach ($language->_states as $stateName => $state) {
//            $prefixedRules = array();
//
//            if (strstr($stateName, ' ')) {
//                $parts = explode(' ', $stateName);
//                $prefixed = array();
//                foreach ($parts as $part)
//                    $prefixed[] = "$prefix$part";
//                $stateName = implode(' ', $prefixed);
//            }
//            else
//                $stateName = "$prefix$stateName";
//
//            foreach ($state as $key => $rule) {
//                if (is_string($key) and is_array($rule)) {
//                    $nestedRules = array();
//                    foreach ($rule as $nestedRule)
//                        $nestedRules[] = ($nestedRule === '') ? '' :
//                                         "$prefix$nestedRule";
//
//                    $prefixedRules["$prefix$key"] = $nestedRules;
//                }
//                else
//                    $prefixedRules[] = "$prefix$rule";
//            }
//
//            if ($stateName === 'init')
//                $prefixedRules = array_merge($hoistBackRules, $prefixedRules);
//
//            $states[$stateName] = $prefixedRules;
//        }
//
//        $rules = array();   // Step 2: rules
//        // Mappings need to set up already!
//        $mappings = array();
//
//        foreach ($language->_rules as $ruleName => $rule) {
//            if (is_array($rule)) {
//                $nestedRules = array();
//                foreach ($rule as $nestedName => $nestedRule) {
//                    if (is_string($nestedName)) {
//                        $nestedRules["$prefix$nestedName"] = $nestedRule;
//                        $mappings["$prefix$nestedName"] = $nestedName;
//                    }
//                    else
//                        $nestedRules[] = $nestedRule;
//                }
//                $rules["$prefix$ruleName"] = $nestedRules;
//            }
//            else {
//                $rules["$prefix$ruleName"] = $rule;
//                $mappings["$prefix$ruleName"] = $ruleName;
//            }
//        }
//
//        // Step 3: mappings.
//
//        foreach ($language->_mappings as $ruleName => $cssClass) {
//            if (strstr($ruleName, ' ')) {
//                $parts = explode(' ', $ruleName);
//                $prefixed = array();
//                foreach ($parts as $part)
//                    $prefixed[] = "$prefix$part";
//                $mappings[implode(' ', $prefixed)] = $cssClass;
//            }
//            else
//                $mappings["$prefix$ruleName"] = $cssClass;
//        }
//
//        $this->addStates($states);
//        $this->addRules($rules);
//        $this->addMappings($mappings);
//
//        return $prefix . 'init';
//    }

    private function makeCompiledLanguage() {
        return new HyperlightCompiledLanguage(
            $this->id(),
            $this->_info,
            $this->_extensions,
            $this->_states,
            $this->_rules,
            $this->_mappings,
            $this->_caseInsensitive,
            $this->_postProcessors
        );
    }

    private static function mergeProperties(array $old, array $new) {
        foreach ($new as $key => $value) {
            if (is_string($key)) {
                if (isset($old[$key]) and is_array($old[$key]))
                    $old[$key] = array_merge($old[$key], $new);
                else
                    $old[$key] = $value;
            }
            else
                $old[] = $value;
        }

        return $old;
    }
}

class HyperlightCompiledLanguage {
    private $_id;
    private $_info;
    private $_extensions;
    private $_states;
    private $_rules;
    private $_mappings;
    private $_caseInsensitive;
    private $_postProcessors = array();

    public function __construct($id, $info, $extensions, $states, $rules, $mappings, $caseInsensitive, $postProcessors) {
        $this->_id = $id;
        $this->_info = $info;
        $this->_extensions = $extensions;
        $this->_caseInsensitive = $caseInsensitive;
        $this->_states = $this->compileStates($states);
        $this->_rules = $this->compileRules($rules);
        $this->_mappings = $mappings;

        foreach ($postProcessors as $ppkey => $ppvalue)
            $this->_postProcessors[$ppkey] = HyperLanguage::compile($ppvalue);
    }

    public function id() {
        return $this->_id;
    }

    public function name() {
        return $this->_info[HyperLanguage::NAME];
    }

    public function authorName() {
        if (!array_key_exists(HyperLanguage::AUTHOR, $this->_info))
            return null;
        $author = $this->_info[HyperLanguage::AUTHOR];
        if (is_string($author))
            return $author;
        if (!array_key_exists(HyperLanguage::NAME, $author))
            return null;
        return $author[HyperLanguage::NAME];
    }

    public function authorWebsite() {
        if (!array_key_exists(HyperLanguage::AUTHOR, $this->_info) or
            !is_array($this->_info[HyperLanguage::AUTHOR]) or
            !array_key_exists(HyperLanguage::WEBSITE, $this->_info[HyperLanguage::AUTHOR]))
            return null;
        return $this->_info[HyperLanguage::AUTHOR][HyperLanguage::WEBSITE];
    }

    public function authorEmail() {
        if (!array_key_exists(HyperLanguage::AUTHOR, $this->_info) or
            !is_array($this->_info[HyperLanguage::AUTHOR]) or
            !array_key_exists(HyperLanguage::EMAIL, $this->_info[HyperLanguage::AUTHOR]))
            return null;
        return $this->_info[HyperLanguage::AUTHOR][HyperLanguage::EMAIL];
    }

    public function authorContact() {
        $email = $this->authorEmail();
        return $email !== null ? $email : $this->authorWebsite();
    }

    public function extensions() {
        return $this->_extensions;
    }

    public function state($stateName) {
        return $this->_states[$stateName];
    }

    public function rule($ruleName) {
        return $this->_rules[$ruleName];
    }

    public function className($state) {
        if (array_key_exists($state, $this->_mappings))
            return $this->_mappings[$state];
        else if (strstr($state, ' ') === false)
            // No mapping for state.
            return $state;
        else {
            // Try mapping parts of nested state name.
            $parts = explode(' ', $state);
            $ret = array();

            foreach ($parts as $part) {
                if (array_key_exists($part, $this->_mappings))
                    $ret[] = $this->_mappings[$part];
                else
                    $ret[] = $part;
            }

            return implode(' ', $ret);
        }
    }

    public function postProcessors() {
        return $this->_postProcessors;
    }

    private function compileStates($states) {
        $ret = array();

        foreach ($states as $name => $state) {
            $newstate = array();

            if (!is_array($state))
                $state = array($state);

            foreach ($state as $key => $elem) {
                if ($elem === null)
                    continue;
                if (is_string($key)) {
                    if (!is_array($elem))
                        $elem = array($elem);

                    foreach ($elem as $el2) {
                        if ($el2 === '')
                            $newstate[] = $key;
                        else
                            $newstate[] = "$key $el2";
                    }
                }
                else
                    $newstate[] = $elem;
            }

            $ret[$name] = $newstate;
        }

        return $ret;
    }

    private function compileRules($rules) {
        $tmp = array();

        // Preprocess keyword list and flatten nested lists:

        // End of regular expression matching keywords.
        $end = $this->_caseInsensitive ? ')\b/i' : ')\b/';

        foreach ($rules as $name => $rule) {
            if (is_array($rule)) {
                if (self::isAssocArray($rule)) {
                    // Array is a nested list of rules.
                    foreach ($rule as $key => $value) {
                        if (is_array($value))
                            // Array represents a list of keywords.
                            $value = '/\b(?:' . implode('|', $value) . $end;

                        if (!is_string($key) or strlen($key) === 0)
                            $tmp[$name] = $value;
                        else
                            $tmp["$name $key"] = $value;
                    }
                }
                else {
                    // Array represents a list of keywords.
                    $rule = '/\b(?:' . implode('|', $rule) . $end;
                    $tmp[$name] = $rule;
                }
            }
            else {
                $tmp[$name] = $rule;
            } // if (is_array($rule))
        } // foreach

        $ret = array();

        foreach ($this->_states as $name => $state) {
            $regex_rules = array();
            $regex_names = array();
            $nesting_rules = array();

            foreach ($state as $rule_name) {
                $rule = $tmp[$rule_name];
                if ($rule instanceof Rule)
                    $nesting_rules[$rule_name] = $rule;
                else {
                    $regex_rules[] = $rule;
                    $regex_names[] = $rule_name;
                }
            }

            $ret[$name] = array_merge(
                array(preg_merge('|', $regex_rules, $regex_names)),
                $nesting_rules
            );
        }

        return $ret;
    }

    private static function isAssocArray(array $array) {
        foreach($array as $key => $_)
            if (is_string($key))
                return true;
        return false;
    }
}

class Hyperlight {
    private $_lang;
    private $_result;
    private $_states;
    private $_omitSpans;
    private $_postProcessors = array();

    public function __construct($lang) {
        if (is_string($lang))
            $this->_lang = HyperLanguage::compileFromName(strtolower($lang));
        else if ($lang instanceof HyperlightCompiledLanguage)
            $this->_lang = $lang;
        else if ($lang instanceof HyperLanguage)
            $this->_lang = HyperLanguage::compile($lang);
        else
            trigger_error(
                'Invalid argument type for $lang to Hyperlight::__construct',
                E_USER_ERROR
            );


		if (method_exists($this->_lang,'postProcessors')) {
			foreach ($this->_lang->postProcessors() as $ppkey => $ppvalue)
	            $this->_postProcessors[$ppkey] = new Hyperlight($ppvalue);
		}


        $this->reset();
    }

    public function language() {
        return $this->_lang;
    }

    public function reset() {
        $this->_states = array('init');
        $this->_omitSpans = array();
    }

    public function render($code) {
		if (false === $this->language()) {
			$this->_lang = HyperLanguage::compileFromName('code');
			$this->reset();
			if (!$this->language()) return htmlspecialchars($code);
		}

        // Normalize line breaks.
        $this->_code = preg_replace('/\r\n?/', "\n", $code);
        $fm = hyperlight_calculate_fold_marks($this->_code, $this->language()->id());
        return hyperlight_apply_fold_marks($this->renderCode(), $fm);
    }

    public function renderAndPrint($code) {
        echo $this->render($code);
    }


    private function renderCode() {
        $code = $this->_code;
        $pos = 0;
        $len = strlen($code);
        $this->_result = '';
        $state = array_peek($this->_states);

        // If there are open states (reentrant parsing), open the corresponding
        // tags first:

        for ($i = 1; $i < count($this->_states); ++$i)
            if (!$this->_omitSpans[$i - 1]) {
                $class = $this->_lang->className($this->_states[$i]);
                $this->write("<span class=\"$class\">");
            }

        // Emergency break to catch faulty rules.
        $prev_pos = -1;

        while ($pos < $len) {
            // The token next to the current position, after the inner loop completes.
            // i.e. $closest_hit = array($matched_text, $position)
            $closest_hit = array('', $len);
            // The rule that found this token.
            $closest_rule = null;
            $rules = $this->_lang->rule($state);

            foreach ((array)$rules as $name => $rule) {
                if ($rule instanceof Rule)
                    $this->matchIfCloser(
                        $rule->start(), $name, $pos, $closest_hit, $closest_rule
                    );
                else if (!empty($rule) && preg_match($rule, $code, $matches, PREG_OFFSET_CAPTURE, $pos) == 1) {
                    // Search which of the sub-patterns matched.
                    foreach ($matches as $group => $match) {
                        if (!is_string($group))
                            continue;
                        if ($match[1] !== -1) {
                            $closest_hit = $match;
                            $closest_rule = str_replace('_', ' ', $group);
                            break;
                        }
                    }
                }
            } // foreach ($rules)

            // If we're currently inside a rule, check whether we've come to the
            // end of it, or the end of any other rule we're nested in.

            if (count($this->_states) > 1) {
                $n = count($this->_states) - 1;
                do {
                    $rule = $this->_lang->rule($this->_states[$n - 1]);
                    $rule = $rule[$this->_states[$n]];
                    --$n;
                    if ($n < 0)
                        throw new NoMatchingRuleException($this->_states, $pos, $code);
                } while ($rule->end() === null);

                $this->matchIfCloser($rule->end(), $n + 1, $pos, $closest_hit, $closest_rule);
            }

            // We take the closest hit:

            if ($closest_hit[1] > $pos)
                $this->emit(substr($code, $pos, $closest_hit[1] - $pos));

            $prev_pos = $pos;
            $pos = $closest_hit[1] + strlen($closest_hit[0]);

            if ($prev_pos === $pos and is_string($closest_rule))
                if (array_key_exists($closest_rule, $this->_lang->rule($state))) {
                    array_push($this->_states, $closest_rule);
                    $state = $closest_rule;
                    $this->emitPartial('', $closest_rule);
                }

            if ($closest_hit[1] === $len)
                break;
            else if (!is_string($closest_rule)) {
                // Pop state.
                if (count($this->_states) <= $closest_rule)
                    throw new NoMatchingRuleException($this->_states, $pos, $code);

                while (count($this->_states) > $closest_rule + 1) {
                    $lastState = array_pop($this->_states);
                    $this->emitPop('', $lastState);
                }
                $lastState = array_pop($this->_states);
                $state = array_peek($this->_states);
                $this->emitPop($closest_hit[0], $lastState);
            }
            else if (array_key_exists($closest_rule, $this->_lang->rule($state))) {
                // Push state.
                array_push($this->_states, $closest_rule);
                $state = $closest_rule;
                $this->emitPartial($closest_hit[0], $closest_rule);
            }
            else
                $this->emit($closest_hit[0], $closest_rule);
        } // while ($pos < $len)

        // Close any tags that are still open (can happen in incomplete code
        // fragments that don't necessarily signify an error (consider PHP
        // embedded in HTML, or a C++ preprocessor code not ending on newline).

        $omitSpansBackup = $this->_omitSpans;
        for ($i = count($this->_states); $i > 1; --$i)
            $this->emitPop();
        $this->_omitSpans = $omitSpansBackup;

        return $this->_result;
    }

    private function matchIfCloser($expr, $next, $pos, &$closest_hit, &$closest_rule) {
        $matches = array();
        if (preg_match($expr, $this->_code, $matches, PREG_OFFSET_CAPTURE, $pos) == 1) {
            if (
                (
                    // Two hits at same position -- compare length
                    // For equal lengths: first come, first serve.
                    $matches[0][1] == $closest_hit[1] and
                    strlen($matches[0][0]) > strlen($closest_hit[0])
                ) or
                $matches[0][1] < $closest_hit[1]
            ) {
                $closest_hit = $matches[0];
                $closest_rule = $next;
            }
        }
    }

    private function processToken($token) {
        if ($token === '')
            return '';
        $nest_lang = array_peek($this->_states);
        if (array_key_exists($nest_lang, $this->_postProcessors))
            return $this->_postProcessors[$nest_lang]->render($token);
        else
            #return self::htmlentities($token);
            return htmlspecialchars($token, ENT_NOQUOTES);
    }

    private function emit($token, $class = '') {
        $token = $this->processToken($token);
        if ($token === '')
            return;
        $class = $this->_lang->className($class);
        if ($class === '')
            $this->write($token);
        else
            $this->write("<span class=\"$class\">$token</span>");
    }

    private function emitPartial($token, $class) {
        $token = $this->processToken($token);
        $class = $this->_lang->className($class);
        if ($class === '') {
            if ($token !== '')
                $this->write($token);
            array_push($this->_omitSpans, true);
        }
        else {
            $this->write("<span class=\"$class\">$token");
            array_push($this->_omitSpans, false);
        }
    }

    private function emitPop($token = '', $class = '') {
        $token = $this->processToken($token);
        if (array_pop($this->_omitSpans))
            $this->write($token);
        else
            $this->write("$token</span>");
    }

    private function write($text) {
        $this->_result .= $text;
    }

//      // DAMN! What did I need them for? Something to do with encoding …
//      // but why not use the `$charset` argument on `htmlspecialchars`?
//    private static function htmlentitiesCallback($match) {
//        switch ($match[0]) {
//            case '<': return '&lt;';
//            case '>': return '&gt;';
//            case '&': return '&amp;';
//        }
//    }
//
//    private static function htmlentities($text) {
//        return htmlspecialchars($text, ENT_NOQUOTES);
//        return preg_replace_callback(
//            '/[<>&]/', array('Hyperlight', 'htmlentitiesCallback'), $text
//        );
//    }
} // class Hyperlight

/**
 * <var>echo</var>s a highlighted code.
 *
 * For example, the following
 * <code>
 * hyperlight('<?php echo \'Hello, world\'; ?>', 'php');
 * </code>
 * results in:
 * <code>
 * <pre class="source-code php">...</pre>
 * </code>
 *
 * @param string $code The code.
 * @param string $lang The language of the code.
 * @param string $tag The surrounding tag to use. Optional.
 * @param array $attributes Attributes to decorate {@link $tag} with.
 *          If no tag is given, this argument can be passed in its place. This
 *          behaviour will be assumed if the third argument is an array.
 *          Attributes must be given as a hash of key value pairs.
 */
function hyperlight($code, $lang, $tag = 'pre', array $attributes = array()) {
    if ($code == '')
        die("`hyperlight` needs a code to work on!");
    if ($lang == '')
        die("`hyperlight` needs to know the code's language!");
    if (is_array($tag) and !empty($attributes))
        die("Can't pass array arguments for \$tag *and* \$attributes to `hyperlight`!");
    if ($tag == '')
        $tag = 'pre';
    if (is_array($tag)) {
        $attributes = $tag;
        $tag = 'pre';
    }
    $lang = htmlspecialchars(strtolower($lang));
    $class = "source-code $lang";

    $attr = array();
    foreach ($attributes as $key => $value) {
        if ($key == 'class')
            $class .= ' ' . htmlspecialchars($value);
        else
            $attr[] = htmlspecialchars($key) . '="' .
                      htmlspecialchars($value) . '"';
    }

    $attr = empty($attr) ? '' : ' ' . implode(' ', $attr);

    $hl = new Hyperlight($lang);
    echo "<$tag class=\"$class\"$attr>";
    $hl->renderAndPrint(trim($code));
    echo "</$tag>";
}

/**
 * Is the same as:
 * <code>
 * hyperlight(file_get_contents($filename), $lang, $tag, $attributes);
 * </code>
 * @see hyperlight()
 */
function hyperlight_file($filename, $lang = null, $tag = 'pre', array $attributes = array()) {
    if ($lang == '') {
        // Try to guess it from file extension.
        $pos = strrpos($filename, '.');
        if ($pos !== false) {
            $ext = substr($filename, $pos + 1);
            $lang = HyperLanguage::nameFromExt($ext);
        }
    }
    hyperlight(file_get_contents($filename), $lang, $tag, $attributes);
}

if (defined('HYPERLIGHT_SHORTCUT')) {
    function hy() {
        $args = func_get_args();
        call_user_func_array('hyperlight', $args);
    }
    function hyf() {
        $args = func_get_args();
        call_user_func_array('hyperlight_file', $args);
    }
}

function hyperlight_calculate_fold_marks($code, $lang) {
    $supporting_languages = array('csharp', 'vb');

    if (!in_array($lang, $supporting_languages))
        return array();

    $fold_begin_marks = array('/^\s*#Region/', '/^\s*#region/');
    $fold_end_marks = array('/^\s*#End Region/', '/\s*#endregion/');

    $lines = preg_split('/\r|\n|\r\n/', $code);

    $fold_begin = array();
    foreach ($fold_begin_marks as $fbm)
        $fold_begin = $fold_begin + preg_grep($fbm, $lines);

    $fold_end = array();
    foreach ($fold_end_marks as $fem)
        $fold_end = $fold_end + preg_grep($fem, $lines);

    if (count($fold_begin) !== count($fold_end) or count($fold_begin) === 0)
        return array();

    $fb = array();
    $fe = array();
    foreach ($fold_begin as $line => $_)
        $fb[] = $line;

    foreach ($fold_end as $line => $_)
        $fe[] = $line;

    $ret = array();
    for ($i = 0; $i < count($fb); $i++)
        $ret[$fb[$i]] = $fe[$i];

    return $ret;
}

function hyperlight_apply_fold_marks($code, array $fold_marks) {
    if ($fold_marks === null or count($fold_marks) === 0)
        return $code;

    $lines = explode("\n", $code);

    foreach ($fold_marks as $begin => $end) {
        $lines[$begin] = '<span class="fold-header">' . $lines[$begin] . '<span class="dots"> </span></span>';
        $lines[$begin + 1] = '<span class="fold">' . $lines[$begin + 1];
        $lines[$end + 1] = '</span>' . $lines[$end + 1];
    }

    return implode("\n", $lines);
}

?>
