(() => {
    "use strict"
    async function applePayButtonClicked(event)
    {
        const applePayMethod = {
            supportedMethods: "https://apple.com/apple-pay",
            data: {
                version: 1,
                merchantIdentifier: "org.webkit.demo",
                merchantCapabilities: ["supports3DS", "supportsCredit", "supportsDebit"],
                supportedNetworks: ["amex", "discover", "masterCard", "visa"],
                countryCode: "US",
            },
        };

        const detailsForShippingOption = selectedShippingOption => {
            const shippingOptions = [
                {
                    id: "ground",
                    label: "Ground Shipping",
                    amount: { value: "5.00", currency: "USD" },
                    overrideTotal: { value: "25.00", currency: "USD" },
                },
                {
                    id: "express",
                    label: "Express Shipping",
                    amount: { value: "10.00", currency: "USD" },
                    overrideTotal: { value: "30.00", currency: "USD" },
                },
            ];

            var shippingOptionIndex = null;
            for (var shippingOption in shippingOptions) {
                if (shippingOptions[shippingOption].id !== selectedShippingOption)
                    continue;
                shippingOptionIndex = shippingOption;
                break;
            }

            if (!shippingOptionIndex)
                return { };

            shippingOptions[shippingOptionIndex].selected = true;

            return {
                total: {
                    label: "WebKit",
                    amount: shippingOptions[shippingOptionIndex].overrideTotal,
                },
                displayItems: [
                    {
                        label: shippingOptions[shippingOptionIndex].label,
                        amount: shippingOptions[shippingOptionIndex].amount,
                    },
                ],
                shippingOptions: shippingOptions,
            };
        };

        const options = {
            requestShipping: true,
        };

        const paymentRequest = new PaymentRequest([applePayMethod], detailsForShippingOption("ground"), options);

        paymentRequest.onmerchantvalidation = event => {
            fetch("merchant-validation.php", {
                body: JSON.stringify({ validationURL: event.validationURL }),
                method: "POST",
            }).then(response => event.complete(response.json()));
        };

        paymentRequest.onshippingaddresschange = event => {
            const detailsUpdate = detailsForShippingOption(paymentRequest.shippingOption);
            event.updateWith(detailsUpdate);
        };

        paymentRequest.onshippingoptionchange = event => {
            const detailsUpdate = detailsForShippingOption(paymentRequest.shippingOption);
            event.updateWith(detailsUpdate);
        };

        const response = await paymentRequest.show();
        response.complete("success");
    }

    window.addEventListener("DOMContentLoaded", () => {
        const applePayButton = document.querySelector("#live-button");
        if (!window.PaymentRequest || !window.ApplePaySession || !ApplePaySession.canMakePayments())
            applePayButton.classList.add("apple-pay-not-supported");
        else {
            applePayButton.classList.add("apple-pay-button");
            applePayButton.addEventListener("click", applePayButtonClicked);   
        }
        applePayButton.classList.remove("hidden");
    });
})();
