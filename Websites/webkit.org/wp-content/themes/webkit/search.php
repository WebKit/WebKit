<?php
get_header();

function d($data) {
    echo "<pre>";
    print_r($data);
    echo "</pre>";
}

define('CONTENT_SUMMARY_TEXT_RANGE', 300);

function dom_markup_term($term, &$Document_or_Node) {
    $Document = $Document_or_Node->ownerDocument ? $Document_or_Node->ownerDocument : $Document_or_Node;
    
    if (!$Document_or_Node->hasChildNodes())
        return;

    foreach ($Document_or_Node->childNodes as $i => $Node) {  
        // Avoid reprocessing already processed nodes
        if ($Node->hasAttributes() && $Node->getAttribute("class") == "search-term")
            continue;

        // Process child nodes with recursion
        if ($Node->hasChildNodes())
            dom_markup_term($term, $Node);
        
        // Only process the raw text nodes`
        if ($Node->nodeType != XML_TEXT_NODE)
            continue;

        // Only process nodes that actually match the term
        $position = mb_stripos($Node->textContent, $term);
        if (false === $position)
            continue;

        $term_text_node = $Node;

        if ($position !== -1) {
            $term_text_node = $Node->splitText($position);
            $ending = $term_text_node->splitText(strlen($term));
        }

        $term_node = $Document->createElement("span", $term_text_node->textContent);
        $term_node->setAttribute("class","search-term");
        if ($Node->parentNode)
            $Node->parentNode->replaceChild($term_node, $term_text_node);
    }
}

function string_markup_term($term, $string) {
    if (false === mb_stripos($string, $term))
        return $string;

    $Document = new DOMDocument();
    libxml_use_internal_errors(true);
    $Document->loadHTML("<p>" . $string . "</p>", LIBXML_HTML_NOIMPLIED | LIBXML_HTML_NODEFDTD | LIBXML_COMPACT);
    
    libxml_clear_errors();

    dom_markup_term($term, $Document);

    return trim(str_replace(["<p>", "</p>"], "", $Document->saveHTML()));
}

function convert_to_inline_elements($element) {
    if ($element->nodeType === XML_TEXT_NODE)
        return $element;
    
    $inline_tag_list = ["span", "code", "strong", "em"];
    if (isset($element->tagName) && in_array($element->tagName, $inline_tag_list))
        return $element;
    
    $Document = $element->ownerDocument;
    if (!$Document)
        return $element;
    
    $parent = $element->parentNode;
    $span = $Document->createElement("span");
    $parent->insertBefore($span, $element);

    if ($element->hasChildNodes()) {
        $child_nodes = $element->childNodes;

        while ($child_nodes->length > 0) {
            $item = $child_nodes->item(0);

            if (isset($item->tagName) && !in_array($item->tagName, $inline_tag_list)) {
                $item = convert_to_inline_elements($item);
            }
            
            $span->appendChild($item);
        }
    }
    
    if ($element->hasAttributes()) {
        $attributes = $element->attributes;
        while ($attributes->length > 0) {
            $attribute = $attributes->item(0);
            if (in_array($attribute->name, ["class", "style"]))
                $attribute->value = "";
            if (!is_null($attribute->namespaceURI)) {
                $span->setAttributeNS('http://www.w3.org/2000/xmlns/',
                                          'xmlns:' . $attribute->prefix,
                                          $attribute->namespaceURI);
            }
            $span->setAttributeNode($attribute);
        }
    }

    // Prettify list items
    if (isset($parent->tagName) && in_array($parent->tagName, ["ol", "ul"]) && $element->tagName == "li") {
        $span->setAttribute("class", "inline-list-item");
        $prefix_string = $Node->tagName == "ol" ? " " . ($list_number++) . ". " : " • ";
        $symbol = $Document->createTextNode($prefix_string);
        $span->insertBefore($symbol, $span->firstChild);
    }
    
    $parent->removeChild($element);
    return $span;
}

function get_child_text_node($Node, $child) {
    $target = $Node->$child;
    while ($target->nodeType !== XML_TEXT_NODE) {
        if (method_exists($target, 'hasChildNodes') && $target->hasChildNodes()) {
            $target = $target->$child;
        } else break;
    }
    return $target;
}

function deep_prune_empty_nodes(&$Node) {
    $empty_nodes = [];
    
    foreach ($Node->childNodes as $i => $child) {
        if (empty($child->textContent))
            $empty_nodes[] = $child;
        elseif ($child->hasChildNodes())
            deep_prune_empty_nodes($child);
    }

    foreach ($empty_nodes as $empty_node)
        $Node->removeChild($empty_node);
}

function trim_node_endings(&$Node) {
    $operations = [
        "firstChild" => "/^[\W\s]+\b/",
        "lastChild" => "/[\b|\W][\W\s]+$/"
    ];

    foreach ($operations as $child => $pattern) {
        if (false === preg_match($pattern, $Node->textContent))
            continue;
        
        $target = get_child_text_node($Node, $child);
        
        if (trim($target->textContent) == "") {
            $empty_node = $target;
            if ($empty_node->nextSibling)
                $target = get_child_text_node($empty_node->nextSibling, $child);
            elseif ($empty_node->previousSibling)
                $target = get_child_text_node($empty_node->previousSibling, $child);
        }
        
        if (method_exists($target, 'textContent'))
            $target->textContent = preg_replace($pattern, "", $target->textContent);
    }
    
    deep_prune_empty_nodes($Node);
}

function maybe_add_ellipses($Node) {
    if (!$Node)
        return;

    $Document = $Node->ownerDocument;
    
    trim_node_endings($Node);
    
    // Add ellipses to at the beginning when starting at a mid-point of a sentence
    if (preg_match("/^[a-z]/", trim($Node->textContent)))
        $Node->insertBefore($Document->createEntityReference("hellip"), $Node->firstChild);

    // Don't do anything else if the node text ends in a full-stop
    if (in_array(substr(trim($Node->textContent), -1), [".", "?", "!"]))
        return;

    // Add an ellipses at the end
    $Node->appendChild($Document->createEntityReference("hellip"));
}

function find_node_matching_most_terms($terms, $Document) {
    $term_count = count($terms);
    $scoremap = [];
    // Scan document for terms, scoring higher for both terms
    foreach ($Document->firstChild->childNodes as $i => $Node) {
        $score = 0;
        // Exact match wins big
        if (false !== mb_stripos($Node->textContent, get_search_query(true)))
            $score += 100;
        
        // Individual term matches bump the score a bit
        foreach ($terms as $term)
            if (false !== mb_stripos($Node->textContent, $term))
                $score += 10;
    
        if ($score > 0 && !isset($scoremap[$score]))
            $scoremap[$score] = $i;
        
        // Stop when there is strong enough signal
        if ($score >= $term_count * 10) break;
    }
    
    // Sort and grab the big winner
    arsort($scoremap);
    $bestnode = reset($scoremap);
    
    if (isset($Document->firstChild->childNodes[$bestnode]))
        return $Document->firstChild->childNodes[$bestnode];
    return false;
}

function trim_beginning_of_node($Node, $terms) {
    // find where to cut, which node to cut, then cut the node
    $length = mb_strlen($Node->textContent);
    if ($length == 0)
        return $Node;
    $position = $length;
    
    if (!$Node->hasChildNodes()) {
        $text = $Node->textContent;
        foreach ($terms as $term)
            $position = min($position, mb_stripos($text, $term));

        $fragment = substr($text, 0, $position);
        $intro = end(preg_split('/[.?!]\s+/', $fragment));
        $position -= mb_strlen($intro);

        if ($position !== -1)
            return $Node->splitText($position);
    }
        
    return $Node;
}

function deep_prune_excess_nodes (&$Node, $length = 0, $limit) {
    $excess_nodes = [];
    
    foreach($Node->childNodes as $i => $child) {
        if ($child->hasChildNodes()) {
            $length += deep_prune_excess_nodes($child, $length, $limit);
        } elseif ($length > $limit) {
            $position = $length - $limit;
            $excess_nodes[] = $child;
        } else $length += $child->length;
    }
    
    foreach ($excess_nodes as $extra_node) {
        $Node->removeChild($extra_node);
    }
     
    return $length;
}

function trim_end_of_node($Node, $overrun) {
    if ($Node->hasChildNodes()) {
        $limit = mb_strlen($Node->textContent) + $overrun;
        deep_prune_excess_nodes($Node, 0, $limit);
    } else {
        $text = $Node->textContent;
        $capture = mb_substr($text, 0, mb_strlen($text) + $overrun);
        $ending = mb_substr($text, $overrun);
        $capture .= preg_replace("/\W.*$/us", "", $ending);
        $offset = mb_strlen($capture);
        $Node->splitText($offset);
        return $Node;
    }
    return $Node;
}

function build_summary_node_list(&$Summary, $Node, $terms) {
    $Document = $Summary->ownerDocument;
    
    $nodelist = [];
    $remaining_length = CONTENT_SUMMARY_TEXT_RANGE;
    
    $nextnode = convert_to_inline_elements($Node);
    $nextnode = trim_beginning_of_node($nextnode, $terms);
    
    $offset = 0;
    
    // If the matching node is too short, add more nodes
    while ($remaining_length > 0) {
        if (!$nextnode) break;
        
        $remaining_length -= strlen($nextnode->textContent);
        
        if ($remaining_length < 0)
            $nextnode = trim_end_of_node($nextnode, $remaining_length);
        
        $nodelist[] = $nextnode;
        $nextnode = convert_to_inline_elements($nextnode->nextSibling);
        
        if (!preg_match('/^\W{1}/', $nextnode->textContent))
            $nodelist[] = $Document->createTextNode(' ');
    }

    // Move modified nodes to the summary node
    foreach ($nodelist as $included_node)
        $Summary->appendChild($included_node);
    
    // Append ellipses where it makes sense
    maybe_add_ellipses($Summary);
    
    return $Summary;
}

function get_search_summary() {
    // Get the post data that needs processed
    $title = mb_convert_encoding(get_the_title(), 'html-entities', 'utf-8');
    $content = mb_convert_encoding(get_the_content(), 'html-entities', 'utf-8');
    $terms = preg_split('/("[^"]*"|\'[^\']\')|\h+/', get_search_query(false), -1, PREG_SPLIT_NO_EMPTY|PREG_SPLIT_DELIM_CAPTURE);
    $term_count = count($terms);
    $first_term = $terms[0];

    libxml_use_internal_errors(true);
    
    // Build a DOM Document for the content
    $Document = new DOMDocument();
    $Document->loadHTML("<div>$content</div>", LIBXML_HTML_NOIMPLIED | LIBXML_HTML_NODEFDTD | LIBXML_COMPACT);
    
    libxml_clear_errors();
    
    // Add a summary container that will collect the summarized part of the post content
    $SummaryElement = $Document->createElement("p");
    $Document->appendChild($SummaryElement);
    
    $Node = find_node_matching_most_terms($terms, $Document);
    if ($Node) {
        build_summary_node_list($SummaryElement, $Node, $terms);

        // Markup the term in the title and the DOM
        foreach ($terms as $term) {
            $term = trim($term, '"\'');
            $title = string_markup_term($term, $title);
            dom_markup_term($term, $SummaryElement);
        }
        
        $summary = $SummaryElement->ownerDocument->saveHTML($SummaryElement);
    } else $summary = get_the_excerpt();

    unset($Document);
    
    return [$title, $summary];
}
?>
        <h1>Search Results</h1>
        <form action="/" method="get">
            <input type="search" name="s" class="search-input" value="<?php echo get_search_query(true); ?>">
        </form>
        <?php if ( have_posts() && get_search_query(true) !== ""): ?>
        <ul class="results-list">
            <?php $i = 0; while ( have_posts() ) : the_post();
                // if ($i++ != 2) continue;
                list($title, $summary) = get_search_summary();
            ?>
            <li>
                <h2><a href="<?php the_permalink(); ?>"><?php echo $title; ?></a></h2>
                <?php echo $summary; ?>
            </li>
            <?php endwhile; ?>
        </ul>
        <?php
            the_posts_pagination( array(
                'prev_text'          => __( 'Previous page' ),
                'next_text'          => __( 'Next page' ),
                'before_page_number' => '<span class="meta-nav screen-reader-text">' . __( 'Page' ) . ' </span>',
            ) );
        else: ?>
            <article class="no-results">
            <div class="bodycopy">
                <h2>No matches were found for "<?php echo get_search_query(true); ?>".</h2>
                <p>Suggestions:</p>
                <ul>
                    <li>Check for spelling errors or typos</li>
                    <li>Try more generalized keywords</li>
                </ul>
            </div>
            </article>
        <?php endif; ?>

<?php get_footer(); ?>