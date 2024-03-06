<script lang="js">
import styles from "news-site-css/dist/toggle.module.css";

export default {
    props: {
        id: String,
        label: String,
        onChange: Function,
        checked: Boolean,
    },
    data() {
        return {
            styles,
            isSelected: false,
        }
    },
    mount() {
        this.isSelected = this.checked;
    },
    methods: {
        handleChange(e) {
            this.isSelected = e.target.checked;
            this.onChange(e);
        }
    }
}
</script>

<template>
    <div :class="styles['toggle-outer']">
        <div :class="styles['toggle-description']">
            {{ label }}
        </div>
        <div :class="styles['toggle-container']">
            <label :class="styles.label" :for="`${id}-toggle`">
                <input :id="`${id}-toggle`" type="checkbox" :checked="isSelected" @change="handleChange" />
                <span :class="styles.switch" />
                <div class="visually-hidden">selected: {{ isSelected ? "true" : "false" }}</div>
            </label>
        </div>
    </div>
</template>
