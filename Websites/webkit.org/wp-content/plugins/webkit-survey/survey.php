<style>
.webkit-survey ul {
    list-style: none;
}

.webkit-survey ul li {
    display: block;
    margin: 0 3rem;
    position: relative;
    vertical-align: top;
    box-sizing: border-box;
    overflow: hidden;
    min-height: 62px;
    perspective: 500px;
}

.webkit-survey input[type=text] {
    display: inline-block;
    vertical-align: baseline;
    margin-left: 1rem;
    margin-bottom: 0;
    backface-visibility: hidden;
    transform: rotateX( 90deg );
    transition: transform 500ms ease;
    visibility: hidden;
}

.webkit-survey input[type=radio] {
    margin-top: 0;
    vertical-align: middle;
    transition: opacity 500ms ease;
}

.webkit-survey input[type=radio]:checked ~ input[type=text] {
    transform: rotateX( 0deg );
    visibility: visible;
}

.webkit-survey input[type=radio]:checked,
.webkit-survey input[type=radio]:checked + span {
    opacity: 1;
    color: var(--text-color);
}

.webkit-survey input[type=radio] + span {
    color: var(--text-color-coolgray);
    transition: color 500ms ease;
}

.webkit-survey input[type=submit],
.webkit-survey a.button {
    padding: 1rem 3rem;
}

.webkit-survey label {
    box-sizing: border-box;
    display: inline-block;
    border-radius: 0.3rem;
    padding: 1rem 3rem 1rem 5rem;
    transition: background-color 500ms ease;
    cursor: pointer;
    text-indent: -2.4rem;
}

.webkit-survey ul label:hover {
    background: var(--tile-background-color);
}

.webkit-survey .button-align-right {
    text-align: right;
}
</style>

<form class="webkit-survey" method="POST">
<?php

wp_nonce_field("webkit_survey-" . get_the_ID(), "_nonce", $true);
foreach ($Survey->survey as $id => $Entry):
?>
    <h3><?php echo $Entry->question; ?></h3>
    
    <ul>
    <?php foreach ($Entry->responses as $value => $response): ?>
        <li><label><input type="radio" name="questions[<?php echo esc_attr($id); ?>]" value="<?php echo esc_attr($value); ?>" required> <span><?php echo esc_html($response); ?></span>
        <?php if ($response == "Other:"): ?>
            <input type="text" name="other[<?php echo esc_attr($id); ?>]" maxlength="144">
        <?php endif; ?>
        </label></li>
    <?php endforeach; ?>
    </ul>
<?php endforeach; ?>
<p class="button-align-right">
    <input type="submit" name="Submit" value="Submit" class="submit-button">
</p>
</form>
