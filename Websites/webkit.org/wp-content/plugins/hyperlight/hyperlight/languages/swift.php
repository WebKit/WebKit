<?php
/**
 * Based on Swift 3.0.1 grammars: 
 * https://developer.apple.com/library/content/documentation/Swift/Conceptual/Swift_Programming_Language/zzSummaryOfTheGrammar.html#//apple_ref/doc/uid/TP40014097-CH38-ID458 
 **/
class SwiftLanguage extends HyperLanguage {
    public function __construct() {
        $this->setInfo(array(
            parent::NAME => 'Swift',
            parent::VERSION => '1.0',
            parent::AUTHOR => array(
                parent::NAME => 'Jonathan Davis',
                parent::WEBSITE => 'webkit.org',
                parent::EMAIL => 'jond@webkit.org'
            )
        ));

        $this->setExtensions(array('swift'));

        $this->addStates(array(
            'init' => array(
                'string', 'char', 'number', 'comment',
                'keyword' => array('', 'declaration', 'type', 'modifier','control','literal', 'operator', 'preprocessor', 'macros', 'builtin'),
                'identifier',
                'operator'
            )
        ));

        $this->addRules(array(
            'whitespace' => RULE::ALL_WHITESPACE,
            'operator' => '/[!|%|&|\*|\-\-|\-|\+\+|\+|~|===|==|=|!=|!==|<=|>=|<<=|>>=|>>>=|<>|<|>|!|&&|\|\||\?\:|\*=|\/=|%=|\+=|\-=|&=|\^=|:]/',
            'string' => Rule::C_DOUBLEQUOTESTRING,
            'char' => Rule::C_SINGLEQUOTESTRING,
            'number' => Rule::C_NUMBER,
            'comment' => Rule::C_COMMENT,
            'keyword' => array(
                array(
                    'optional','self','super','import','deinit','import','init','subscript'
                ),
                'declaration' => array(
                    'precedencegroup','class','struct','enum','extension','protocol','let','var','typealias','func','throws','rethrows'
                ),
                'type' => array(
                    'boolean','byte','char','const','double','enum','float','int','interface','long','short','void','typealias','struct','const'
                ),
                'modifier' => array(
                    'get','set','convenience','dynamic','final','infix','lazy','optional','override','postfix','prefix','required','static','unowned','weak','private','fileprivate','internal','public','open','mutating'
                ),
                'control' => array(
                    'try','catch','repeat','break','case','continue','default','do','else','fallthrough','if','in','for','return','switch','where','while','guard','defer'
                ),
                'literal' => array(
                    'false','this','true','nil','none'
                ),
                'operator' => array(
                    'delete','in','instanceof','new','of','typeof','void','with','as','is'
                ),
                'preprocessor' => '/#(?:if|else|endif|elseif|define|undef|warning|error|line|region|endregion)/',
                'macros' => '/@(?:IBAction|IBOutlet|IBDesignable|IBInspectable|warn_unused_result|discardableResult|objc)/',
                'builtin' => '/\b((UI|NS|CF|CG)[A-Z][a-zA-Z0-9]+|Character|U?Int|U?Int(8|16|32|64)|Float|Double|Float(32|64)|Bool|String|Date|Data|URL|Any|AnyObject|Error|Equatable|Hashable|Comparable|CustomDebugStringConvertible|CustomStringConvertible|OptionSet|ManagedBuffer|ManagedBufferPointer|BitwiseOperations|CountedSet|Counter|Directions|ExpressibleByArrayLiteral|ExpressibleByBooleanLiteral|ExpressibleByDictionaryLiteral|ExpressibleByExtendedGraphemeClusterLiteral|ExpressibleByFloatLitera|ExpressibleByIntegerLiteral|ExpressibleByNilLiteral|ExpressibleByStringInterpolation|ExpressibleByStringLiteral|ExpressibleByUnicodeScalarLiteral|OrderedSet|PaperSize|RawRepresentable|(UI|NS|CF|CG)[A-Z][a-zA-Z0-9]+|Stream|(In|Out)putStream|FileManager|Array|Unsafe[a-zA-Z]*Pointer|Bundle|Jex)\b/'
            ),
            'identifier' => Rule::C_IDENTIFIER,
        ));

        $this->addMappings(array(
            'whitespace' => '',
            'identifier' => '',
        ));
    }
}