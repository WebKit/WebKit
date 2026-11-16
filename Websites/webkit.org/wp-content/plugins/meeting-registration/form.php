<style>
#registration .form-field {
    position: relative;
    flex-basis: calc(50% - 1.5rem);
}

#registration input[type=text],
#registration textarea {
    display: inline-block;
    vertical-align: baseline;
    backface-visibility: hidden;
    font-size: 2rem;
    box-sizing: border-box;
    border-radius: 4px;
    border: 1px solid hsl(0, 0%, 83.9%);
    border-color: var(--input-border-color);
    padding: 1.5rem;
    padding-top: 3rem;
    width: 100%;
    margin-bottom: 0rem;
}

#registration textarea {
    padding-top: 2rem;
}

#registration .text-area {
    width: 100%;
    flex: revert;
}

#registration input[type=submit] {
    padding: 1rem 3rem;
}

#registration label {
    position: absolute;
    text-transform: uppercase;
    z-index: 100;
    left: 1.7rem;
    font-size: 1.2rem;
    top: 0.3rem;
    transition: font 100ms ease-out, top 50ms ease-out;
    cursor: pointer;
}

#registration label ~ ul {
    list-style: none;
    padding-left: 0;
    margin: 0;
}

#registration input[type=checkbox],
#registration input[type=radio] {
    vertical-align: middle;
}

input ~ p {
    margin-top: 0.5rem;
    font-size: 1.5rem;
    margin-bottom: 0;
}

@supports selector(label:has(~ input:focus)) {
    #registration label {
        font-size: 1.7rem;
        top: 0.9rem;
    }

    #registration label:has(~ input:focus),
    #registration label:has(~ textarea:focus),
    #registration label:has(~ input:not(:placeholder-shown)),
    #registration label:has(~ textarea:not(:placeholder-shown)) {
        font-size: 1.2rem;
        top: 0.3rem;
    }
}

#registration .form-toggle {
    flex-basis: 100%;
}

#registration .form-toggle .form-label {
    text-transform: revert;
    position: relative;
    top: 0 !important;
    left: 0;
}

#registration .alignright {
    flex-grow: 2;
    text-align: right;
    float: none;
}

@media only screen and (max-width: 415px) {
    #registration .form-field {
        position: relative;
        flex-basis: 100%;
    }

}
</style>
<form action="" method="POST">

    <?php echo WebKit_Meeting_Registration::form_nonce(); ?>

    <div class="form-field">
        <label class="form-label" for="slack-name">Slack Name</label>
        <input type="text" name="slack" id="slack-name" placeholder="&nbsp;">
    </div>

    <div class="form-field">
        <label class="form-label" for="affiliation">Affiliation/Company</label>
        <input type="text" name="affiliation" id="affiliation" placeholder="&nbsp;" list="companies">
        <datalist id="companies">
            <option>Apple Inc.</option>
            <option>Igalia, S.L.</option>
            <option>Google</option>
            <option>Sony Interactive Entertainment</option>
        </datalist>
    </div>

    <div class="form-field text-area">
        <label class="form-label" for="interests">Topics of Interest, Presentation Proposal</label>
        <textarea name="interests" id="interests" rows="7" placeholder="&nbsp;"></textarea>
    </div>
    
    <div class="form-field text-area">
        <label class="form-label" for="bof-topic">Birds of a Feather Topics</label>
        <input type="text" name="boftopics" id="bof-topic" placeholder="&nbsp;">
        <p>We encourage you to suggest a list of topics for Birds of a Feather discussions.</p>
    </div>
    
    <div class="form-field text-area">
        <label class="form-label" for="accessibility-needs">List any Accessibility Needs</label>
        <input type="text" name="accessibility" id="accessibility-needs" placeholder="&nbsp;">
    </div>
    
    <div class="form-field text-area">
        <label class="form-label" for="accessibility-needs">List any special Dietary Needs</label>
        <input type="text" name="dietneeds" id="dietary-needs" placeholder="&nbsp;">
    </div>
    
    <div class="form-field form-toggle">
        <label class="form-label">Please let us know your attendance preference so that we can plan appropriately:</label>
        <ul>
            <li><label class="form-label"><input type="radio" name="attendance" value="in-person"> I will attend in-person.</label></li>
            <li><label class="form-label"><input type="radio" name="attendance" value="online"> I will attend online.</label></li>
        </ul>
    </div>

    
    <div class="form-field form-toggle">
        <label class="form-label" for="contributor-toggle"><input type="checkbox" id="contributor-toggle" name="claim" <?php echo WebKit_Meeting_Registration::is_contributor() ? ' checked="checked"' : ''; ?>> I am a WebKit contributor</label>
    </div>
    


    <div class="alignright">
        <input type="submit" name="Submit" value="Register" class="submit-button">
    </div>
</form>