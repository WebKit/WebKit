function activateThen(completion)
{
    return new Promise(resolve => {
        var button = document.createElement("button");
        button.style["position"] = "absolute";
        button.onclick = () => {
            document.body.removeChild(button);
            resolve(completion());
        };
        document.body.insertBefore(button, document.body.firstChild);
        UIHelper.activateElement(button);
    });
}

function user_activation_test(func, name)
{
    promise_test(async t => {
        await activateThen(() => func(t));
    }, name);
}

function validPaymentMethod()
{
    return {
        supportedMethods: 'https://apple.com/apple-pay',
        data: {
            version: 2,
            merchantIdentifier: '',
            countryCode: 'US',
            supportedNetworks: ['visa', 'masterCard'],
            merchantCapabilities: ['supports3DS'],
        },
    }
}

function validPaymentDetails()
{
    return {
        total: {
            label: 'Total',
            amount: {
                currency: 'USD',
                value: '10.00',
            },
        },
        displayItems: [{
            label: 'Item',
            amount: {
                currency: 'USD',
                value: '10.00',
            },
        }],
    }
}

function updateDetailsOnShippingAddressChange(paymentDetailsInit, paymentOptions, detailsUpdate)
{
    return new Promise((resolve, reject) => {
        activateThen(() => {
            var request = new PaymentRequest([validPaymentMethod()], paymentDetailsInit, paymentOptions);
            request.onmerchantvalidation = (event) => {
                event.complete({ });
            };
            request.onshippingaddresschange = (event) => {
                var detailsUpdatePromise = new Promise((resolve, reject) => {
                    resolve(detailsUpdate);
                });
                event.updateWith(detailsUpdatePromise);
                detailsUpdatePromise.then(() => {
                    resolve();
                    request.abort().catch(() => { });
                });
            };
            request.show().catch(error => error);
        });
    });
}
