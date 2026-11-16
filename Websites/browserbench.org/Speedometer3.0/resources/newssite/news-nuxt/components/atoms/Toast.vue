<script lang="js">
import toastStyles from "news-site-css/dist/toast.module.css";
import buttonStyles from "news-site-css/dist/button.module.css";

export default {
    props: {
        onClose: Function,
        onAccept: Function,
        onReject: Function,
        notification: Object
    },
    data() {
        return {
            toastStyles,
            buttonStyles,
            callbacks: {
                'accept': this.onAccept,
                'reject': this.onReject,
            }
        }
    },
}
</script>

<template>
    <div :class="[toastStyles.toast, toastStyles.open]">
        <button id="close-toast-link" :class="toastStyles['toast-close-button']" title="Close Button" @click="onClose">
            <div :class="[toastStyles['toast-close-button-icon'], 'animated-icon', 'close-icon', 'hover']" title="Close Icon">
                <span class="animated-icon-inner">
                    <span />
                    <span />
                </span>
            </div>
        </button>
        <header v-if="notification.title" :class="toastStyles['toast-header']">
            <h2>{{ notification.title }}</h2>
        </header>
        <section :class="toastStyles['toast-body']">
            <div :class="toastStyles['toast-description']">
                {{ notification.description }}
            </div>
            <div :class="toastStyles['toast-actions']">
                <button
                    v-for="action in notification.actions"
                    :id="`toast-${action.type}-button`"
                    :key="`toast-${action.type}-button`"
                    :class="[buttonStyles.button, buttonStyles[`${action.priority}-button`], toastStyles['toast-actions-button']]"
                    @click="callbacks[action.type]"
                >
                    {{ action.name }}
                </button>
            </div>
        </section>
    </div>
</template>
