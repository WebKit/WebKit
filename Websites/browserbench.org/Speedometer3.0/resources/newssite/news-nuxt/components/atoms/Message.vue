<script lang="js">
import styles from "news-site-css/dist/message.module.css";

export default {
    props: {
        onClose: Function,
        message: Object,
    },
    data() {
        return {
            styles,
        }
    },
}
</script>

<template>
    <div :class="[styles.message, styles.open]">
        <button id="close-message-link" :class="styles['message-close-button']" title="Close Button" @click="onClose">
            <div :class="[styles['message-close-button-icon'], 'animated-icon', 'close-icon', 'hover']" title="Close Icon">
                <span class="animated-icon-inner">
                    <span />
                    <span />
                </span>
            </div>
        </button>
        <header v-if="message.title" :class="styles['message-header']">
            <h2>{{ message.title }}</h2>
        </header>
        <section :class="styles['message-body']">
            <div :class="styles['message-description']">
                {{ message.description }}
            </div>
        </section>
    </div>
</template>
