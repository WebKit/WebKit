/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS""
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

WI.AuthenticationCeremonyDetailView = class AuthenticationCeremonyDetailView extends WI.AuthenticationDetailView
{
    constructor(representedObject, delegate)
    {
        super(representedObject, delegate);

        this.element.classList.add("ceremony");

        this._detailsContentView = null;
        this._requestContentView = null;
        this._responseContentView = null;
    }

    // Protected

    initialLayout()
    {
        this.createDetailNavigationItem("details", WI.UIString("Details"));
        this.createDetailNavigationItem("request", WI.UIString("Request"));

        // Only create response tab if we have response data
        if (this._representedObject?.response)
            this.createDetailNavigationItem("response", WI.UIString("Response"));

        super.initialLayout();
    }

    // Public

    showResponseTab()
    {
        if (!this.detailNavigationItemForIdentifier("response")) {
            this.createDetailNavigationItem("response", WI.UIString("Response"));

            if (this._contentBrowser) {
                let responseNavItem = this.detailNavigationItemForIdentifier("response");
                // Insert after request tab (index 2: close button at 0, details at 1, request at 2, response at 3)
                this._contentBrowser.navigationBar.insertNavigationItem(responseNavItem, 3);
            }
        }
    }

    // Private

    showContentViewForIdentifier(identifier)
    {
        super.showContentViewForIdentifier(identifier);

        switch (identifier) {
        case "details":
            if (!this._detailsContentView)
                this._detailsContentView = this._createDetailsContentView();
            this._contentBrowser.showContentView(this._detailsContentView, this._contentViewCookie);
            break;
        case "request":
            if (!this._requestContentView)
                this._requestContentView = this._createRequestContentView();
            this._contentBrowser.showContentView(this._requestContentView, this._contentViewCookie);
            break;
        case "response":
            if (!this._responseContentView)
                this._responseContentView = this._createResponseContentView();
            this._contentBrowser.showContentView(this._responseContentView, this._contentViewCookie);
            break;
        }

        this._contentViewCookie = null;
    }

    _createDetailsContentView()
    {
        let ceremony = this._representedObject;
        let contentView = new WI.ContentView(null);
        contentView.element.classList.add("authentication-ceremony-details", "resource-details");

        // Source Location Section
        if (ceremony?.initiatorSourceCodeLocation) {
            let sourceSection = new WI.ResourceDetailsSection(WI.UIString("Source Location"), "source-location");
            contentView.element.appendChild(sourceSection.element);

            let sourceCodeLink = WI.createSourceCodeLocationLink(ceremony.initiatorSourceCodeLocation, {
                dontFloat: true,
                ignoreSearchTab: true,
            });

            sourceSection.appendKeyValuePair(WI.UIString("Initiated from"), sourceCodeLink);
        }

        // Summary Section
        let summarySection = new WI.ResourceDetailsSection(WI.UIString("Summary"), "summary");
        contentView.element.appendChild(summarySection.element);

        let ceremonyType = ceremony?.type === "create" ? WI.UIString("Credential Creation") : WI.UIString("Authentication Assertion");
        summarySection.appendKeyValuePair(WI.UIString("Ceremony Type"), ceremonyType);

        let rpId = ceremony?.request?.rp?.id || ceremony?.request?.rpId || WI.UIString("Unknown");
        summarySection.appendKeyValuePair(WI.UIString("Relying Party"), rpId);

        if (ceremony?.request?.rp?.name)
            summarySection.appendKeyValuePair(WI.UIString("RP Name"), ceremony.request.rp.name);

        // User Information Section
        if (ceremony?.request?.user || ceremony?.response?.response?.userHandle) {
            let userSection = new WI.ResourceDetailsSection(WI.UIString("User Information"), "user");
            contentView.element.appendChild(userSection.element);

            if (ceremony?.request?.user?.id)
                this._appendKeyValuePairWithBase64Support(userSection, WI.UIString("User ID"), ceremony.request.user.id);
            if (ceremony?.request?.user?.name)
                userSection.appendKeyValuePair(WI.UIString("User Name"), ceremony.request.user.name);
            if (ceremony?.request?.user?.displayName)
                userSection.appendKeyValuePair(WI.UIString("Display Name"), ceremony.request.user.displayName);
            if (ceremony?.response?.response?.userHandle)
                this._appendKeyValuePairWithBase64Support(userSection, WI.UIString("User Handle"), ceremony.response.response.userHandle);
        }

        // Authenticator Section
        let authenticatorSection = new WI.ResourceDetailsSection(WI.UIString("Authenticator"), "authenticator");
        contentView.element.appendChild(authenticatorSection.element);

        let attachment = ceremony?.request?.authenticatorSelection?.authenticatorAttachment || ceremony?.response?.authenticatorAttachment;
        if (attachment) {
            let attachmentDisplay = attachment === "platform" ? WI.UIString("Platform") :
                                  attachment === "cross-platform" ? WI.UIString("Cross-Platform") : attachment;
            authenticatorSection.appendKeyValuePair(WI.UIString("Attachment"), attachmentDisplay);
        }

        if (ceremony?.request?.authenticatorSelection?.userVerification)
            authenticatorSection.appendKeyValuePair(WI.UIString("User Verification"), ceremony.request.authenticatorSelection.userVerification);

        if (ceremony?.request?.authenticatorSelection?.residentKey)
            authenticatorSection.appendKeyValuePair(WI.UIString("Resident Key"), ceremony.request.authenticatorSelection.residentKey);

        // Request Parameters Section
        let cryptoSection = new WI.ResourceDetailsSection(WI.UIString("Request Parameters"), "request-params");
        contentView.element.appendChild(cryptoSection.element);

        if (ceremony?.request?.challenge)
                this._appendKeyValuePairWithBase64Support(cryptoSection, WI.UIString("Challenge"), String(ceremony.request.challenge));

        if (ceremony?.request?.timeout)
            cryptoSection.appendKeyValuePair(WI.UIString("Timeout"), `${ceremony.request.timeout}ms`);

        if (ceremony?.request?.attestation)
            cryptoSection.appendKeyValuePair(WI.UIString("Attestation"), ceremony.request.attestation);

        // Public Key Credential Parameters
        if (ceremony?.request?.pubKeyCredParams && ceremony.request.pubKeyCredParams.length > 0) {
            let algorithmsSection = new WI.ResourceDetailsSection(WI.UIString("Supported Algorithms"), "algorithms");
            contentView.element.appendChild(algorithmsSection.element);

            ceremony.request.pubKeyCredParams.forEach((param, index) => {
                let algName = this._getAlgorithmName(param.alg);
                algorithmsSection.appendKeyValuePair(`Algorithm ${index + 1}`, `${algName} (${param.alg})`);
            });
        }

        // Extensions
        if (ceremony?.request?.extensions && Object.keys(ceremony.request.extensions).length > 0) {
            let extensionsSection = new WI.ResourceDetailsSection(WI.UIString("Extensions"), "extensions");
            contentView.element.appendChild(extensionsSection.element);

            // Show extensions as collapsible JSON if there are multiple or complex extensions
            if (Object.keys(ceremony.request.extensions).length > 1 ||
                Object.values(ceremony.request.extensions).some(value => typeof value === "object" && value !== null)) {
                let extensionsContainer = JSON.stringify(ceremony.request.extensions, null, 2);
                extensionsSection.appendKeyValuePair(WI.UIString("All Extensions"), extensionsContainer);
            } else {
                // For simple single extensions, show them individually
                for (let [key, value] of Object.entries(ceremony.request.extensions))
                    extensionsSection.appendKeyValuePair(key, JSON.stringify(value));
            }
        }

        return contentView;
    }

    _getAlgorithmName(alg)
    {
        const algorithms = {
            "-7": "ES256 (ECDSA w/ SHA-256)",
            "-35": "ES384 (ECDSA w/ SHA-384)",
            "-36": "ES512 (ECDSA w/ SHA-512)",
            "-257": "RS256 (RSASSA-PKCS1-v1_5 w/ SHA-256)",
            "-258": "RS384 (RSASSA-PKCS1-v1_5 w/ SHA-384)",
            "-259": "RS512 (RSASSA-PKCS1-v1_5 w/ SHA-512)",
            "-37": "PS256 (RSASSA-PSS w/ SHA-256)",
            "-38": "PS384 (RSASSA-PSS w/ SHA-384)",
            "-39": "PS512 (RSASSA-PSS w/ SHA-512)",
            "-8": "EdDSA"
        };
        return algorithms[alg.toString()] || WI.UIString("Unknown Algorithm");
    }

    _isBase64String(str)
    {
        if (!str || typeof str !== "string")
            return false;

        const base64Pattern = /^[A-Za-z0-9+/]*={0,2}$/;
        return base64Pattern.test(str) && str.length > 8;
    }

    _isHexString(str)
    {
        if (!str || typeof str !== "string")
            return false;

        const hexPattern = /^[0-9A-Fa-f]+$/;
        return hexPattern.test(str) && str.length > 8 && str.length % 2 === 0;
    }

    _normalizeBase64url(base64urlStr)
    {
        let base64 = base64urlStr
            .replace(/-/g, "+")
            .replace(/_/g, "/");

        while (base64.length % 4) {
            base64 += "=";
        }

        return base64;
    }

    _base64ToHex(base64Str)
    {
        try {
            let normalizedBase64 = this._normalizeBase64url(base64Str);

            const binaryString = atob(normalizedBase64);
            let hex = "";
            for (let i = 0; i < binaryString.length; i++) {
                const byte = binaryString.charCodeAt(i);
                hex += byte.toString(16).padStart(2, "0");
            }
            return hex.toUpperCase();
        } catch (e) {
            console.error("Failed to convert base64 to hex:", e);
            return null;
        }
    }


    _createBase64ContextMenu(event, value, valueType = "data")
    {
        if (!this._isBase64String(value) && !this._isBase64urlString(value) && !this._isHexString(value))
            return;

        let contextMenu = WI.ContextMenu.createFromEvent(event);

        if (this._isBase64String(value) || this._isBase64urlString(value)) {
            contextMenu.appendItem(WI.UIString("Copy as Base64URL"), () => {
                InspectorFrontendHost.copyText(value);
            });

            let hexValue = this._base64ToHex(value);
            if (hexValue) {
                contextMenu.appendItem(WI.UIString("Copy as Hex"), () => {
                    InspectorFrontendHost.copyText(hexValue);
                });
            }

            contextMenu.appendItem(WI.UIString("Save as File"), () => {
                this._saveBase64AsFile(value, valueType);
            });
        } else if (this._isHexString(value)) {
            contextMenu.appendItem(WI.UIString("Copy as Hex"), () => {
                InspectorFrontendHost.copyText(value);
            });

            let base64Value = this._hexToBase64(value);
            if (base64Value) {
                let base64urlValue = base64Value.replace(/\+/g, "-").replace(/\//g, "_").replace(/=/g, "");
                contextMenu.appendItem(WI.UIString("Copy as Base64URL"), () => {
                    InspectorFrontendHost.copyText(base64urlValue);
                });
            }

            contextMenu.appendItem(WI.UIString("Save as File"), () => {
                this._saveHexAsFile(value, valueType);
            });
        }
    }

    _hexToBase64(hexStr)
    {
        try {
            const bytes = new Uint8Array(hexStr.length / 2);
            for (let i = 0; i < hexStr.length; i += 2) {
                const parsed = parseInt(hexStr.substring(i, i + 2), 16);
                if (isNaN(parsed))
                    return null;
                bytes[i / 2] = parsed;
            }

            let binaryString = "";
            for (let i = 0; i < bytes.length; i++) {
                binaryString += String.fromCharCode(bytes[i]);
            }
            return btoa(binaryString);
        } catch (e) {
            return null;
        }
    }

    _saveHexAsFile(hexValue, valueType = "data")
    {
        try {
            let base64Value = this._hexToBase64(hexValue);
            if (!base64Value) {
                InspectorFrontendAPI.showConsole();
                console.error("Failed to convert hex data for saving");
                return;
            }

            let filename = this._generateFilename(valueType);

            WI.FileUtilities.save(WI.FileUtilities.SaveMode.SingleFile, {
                content: base64Value,
                base64Encoded: true,
                suggestedName: filename
            }, true);
        } catch (e) {
            InspectorFrontendAPI.showConsole();
            console.error("Failed to save hex data as file:", e);
        }
    }

    _generateFilename(valueType)
    {
        let ceremony = this._representedObject;

        let website = "unknown";
        if (ceremony?.request?.rp?.id) {
            website = ceremony.request.rp.id;
        } else if (ceremony?.request?.rpId) {
            website = ceremony.request.rpId;
        } else if (WI.networkManager.mainFrame) {
            try {
                let url = new URL(WI.networkManager.mainFrame.url);
                website = url.hostname;
            } catch (e) {
                website = WI.networkManager.mainFrame.url.split("/")[2] || "unknown";
            }
        }

        website = website.replace(/[^a-zA-Z0-9._-]/g, "_");

        let ceremonyType = ceremony?.type === "create" ? "create" : "get";

        valueType = valueType.toLowerCase().replace(/[^a-zA-Z0-9]/g, "_");

        let now = new Date();
        let timestamp = now.getFullYear()
                       String(now.getMonth() + 1).padStart(2, "0")
                       String(now.getDate()).padStart(2, "0") + "_"
                       String(now.getHours()).padStart(2, "0")
                       String(now.getMinutes()).padStart(2, "0")
                       String(now.getSeconds()).padStart(2, "0");

        let filename = `webauthn_${website}_${ceremonyType}_${valueType}_${timestamp}.bin`;

        return filename;
    }

    _saveBase64AsFile(base64Value, valueType = "data")
    {
        try {
            let normalizedBase64 = this._normalizeBase64url(base64Value);

            let filename = this._generateFilename(valueType);

            WI.FileUtilities.save(WI.FileUtilities.SaveMode.SingleFile, {
                content: normalizedBase64,
                base64Encoded: true,
                suggestedName: filename
            }, true);
        } catch (e) {
            InspectorFrontendAPI.showConsole();
            console.error("Failed to save base64 data as file:", e);
        }
    }

    _appendKeyValuePairWithBase64Support(section, key, value, valueType)
    {
        let valueElement = section.appendKeyValuePair(key, value);

        if (this._isBase64String(value) || this._isBase64urlString(value) || this._isHexString(value)) {
            if (!valueType)
                valueType = this._getValueTypeFromKey(key);

            valueElement.addEventListener("contextmenu", (event) => {
                this._createBase64ContextMenu(event, value, valueType);
            });

            valueElement.style.cursor = "context-menu";
            valueElement.title = WI.UIString("Right-click to copy in different formats");
        }
        
        return valueElement;
    }

    _getValueTypeFromKey(key)
    {
        // Map common key names to descriptive value types
        const keyMappings = {
            "Challenge": "challenge",
            "User ID": "user_id",
            "User Handle": "user_handle",
            "Authenticator Data": "authenticator_data",
            "Signature": "signature",
            "Attestation Object": "attestation_object",
            "Public Key": "public_key",
            "Credential ID": "credential_id",
            "RP ID Hash": "rp_id_hash",
            "Client Data JSON": "client_data_json",
            "Extensions (Raw CBOR)": "extensions",
            "Public Key (Raw CBOR)": "public_key_cbor",
            "Credential Public Key (CBOR)": "credential_public_key"
        };
        
        return keyMappings[key] || "data";
    }

    _getCOSEKeyType(keyType)
    {
        const types = {
            1: "OKP (Octet Key Pair)",
            2: "EC2 (Elliptic Curve)",
            3: "RSA",
            4: "Symmetric",
            5: "HSS-LMS"
        };
        return types[keyType] || `Unknown (${keyType})`;
    }

    _getCOSEAlgorithm(alg)
    {
        const algorithms = {
            "-7": "ES256 (ECDSA w/ SHA-256)",
            "-35": "ES384 (ECDSA w/ SHA-384)",
            "-36": "ES512 (ECDSA w/ SHA-512)",
            "-257": "RS256 (RSASSA-PKCS1-v1_5 w/ SHA-256)",
            "-258": "RS384 (RSASSA-PKCS1-v1_5 w/ SHA-384)",
            "-259": "RS512 (RSASSA-PKCS1-v1_5 w/ SHA-512)",
            "-37": "PS256 (RSASSA-PSS w/ SHA-256)",
            "-38": "PS384 (RSASSA-PSS w/ SHA-384)",
            "-39": "PS512 (RSASSA-PSS w/ SHA-512)",
            "-8": "EdDSA",
            "-65": "ECDH-ES + HKDF-256",
            "-25": "ECDH-ES + HKDF-512",
            "-27": "ECDH-ES + A128KW",
            "-28": "ECDH-ES + A192KW",
            "-29": "ECDH-ES + A256KW"
        };
        return algorithms[alg.toString()] || `Unknown Algorithm (${alg})`;
    }

    _getCOSECurve(curve)
    {
        const curves = {
            1: "P-256",
            2: "P-384",
            3: "P-521",
            4: "X25519",
            5: "X448",
            6: "Ed25519",
            7: "Ed448",
            8: "secp256k1"
        };
        return curves[curve] || `Unknown Curve (${curve})`;
    }

    _interpretWebAuthnExtension(extensionName, value)
    {
        switch (extensionName) {
            case "appid":
                return typeof value === "boolean" ?
                    (value ? WI.UIString("App ID used") : WI.UIString("App ID not used")) :
                    String(value);
                    
            case "appidExclude":
                return typeof value === "boolean" ?
                    (value ? WI.UIString("App ID exclusion applied") : WI.UIString("App ID exclusion not applied")) :
                    String(value);
                    
            case "uvm":
                if (Array.isArray(value)) {
                    return value.map(uv => {
                        if (Array.isArray(uv) && uv.length >= 3) {
                            const method = this._getUserVerificationMethod(uv[0]);
                            const keyProtection = this._getKeyProtection(uv[1]);
                            const matcherProtection = this._getMatcherProtection(uv[2]);
                            return `${method}, ${keyProtection}, ${matcherProtection}`;
                        }
                        return String(uv);
                    }).join("; ");
                }
                return String(value);
                
            case "credProps":
                if (typeof value === "object" && value !== null) {
                    const props = [];
                    if ("rk" in value) props.push(`Resident Key: ${value.rk ? "Yes" : "No"}`);
                    if ("authenticatorDisplayName" in value) props.push(`Display Name: ${value.authenticatorDisplayName}`);
                    return props.join(", ") || WI.UIString("No properties");
                }
                return String(value);
                
            case "largeBlob":
                if (typeof value === "object" && value !== null) {
                    const props = [];
                    if ("supported" in value) props.push(`Supported: ${value.supported ? "Yes" : "No"}`);
                    if ("blob" in value) props.push(`Blob: ${typeof value.blob === "string" ? value.blob : "[Binary Data]"}`);
                    if ("written" in value) props.push(`Written: ${value.written ? "Yes" : "No"}`);
                    return props.join(", ") || WI.UIString("No large blob data");
                }
                return String(value);
                
            case "credBlob":
                return typeof value === "string" ? `Credential Blob: ${value}` : String(value);
                
            case "getCredBlob":
                return typeof value === "string" ? `Retrieved Blob: ${value}` : String(value);
                
            case "minPinLength":
                return typeof value === "number" ? `Minimum PIN Length: ${value}` : String(value);
                
            case "hmac-secret":
                if (typeof value === "object" && value !== null) {
                    const props = [];
                    if ("hmacCreateSecret" in value) props.push(`Create Secret: ${value.hmacCreateSecret ? "Yes" : "No"}`);
                    if ("hmacGetSecret" in value) props.push(`Get Secret: ${typeof value.hmacGetSecret === "string" ? "[Secret Data]" : String(value.hmacGetSecret)}`);
                    return props.join(", ") || WI.UIString("HMAC Secret extension");
                }
                return typeof value === "boolean" ?
                    (value ? WI.UIString("HMAC Secret supported") : WI.UIString("HMAC Secret not supported")) :
                    String(value);
                    
            case "credentialProtectionPolicy":
                const policies = {
                    1: WI.UIString("User verification optional"),
                    2: WI.UIString("User verification optional with credential ID list"),
                    3: WI.UIString("User verification required")
                };
                return typeof value === "number" ? (policies[value] || `Policy ${value}`) : String(value);
                
            case "enforceCredentialProtectionPolicy":
                return typeof value === "boolean" ?
                    (value ? WI.UIString("Credential protection enforced") : WI.UIString("Credential protection not enforced")) :
                    String(value);
                    
            default:
                // Unknown extension - show raw value
                return String(value);
        }
    }

    _getUserVerificationMethod(method)
    {
        const methods = {
            1: WI.UIString("Presence"),
            2: WI.UIString("Fingerprint"),
            4: WI.UIString("Passcode"),
            8: WI.UIString("Voiceprint"),
            16: WI.UIString("Faceprint"),
            32: WI.UIString("Location"),
            64: WI.UIString("Eyeprint"),
            128: WI.UIString("Pattern"),
            256: WI.UIString("Handprint"),
            512: WI.UIString("None"),
            1024: WI.UIString("All")
        };
        return methods[method] || `Method ${method}`;
    }

    _getKeyProtection(protection)
    {
        const protections = {
            1: WI.UIString("Software"),
            2: WI.UIString("Hardware"),
            4: WI.UIString("TEE"),
            8: WI.UIString("Secure Element"),
            16: WI.UIString("Remote Handle")
        };
        return protections[protection] || `Protection ${protection}`;
    }

    _getMatcherProtection(protection)
    {
        const protections = {
            1: WI.UIString("Software"),
            2: WI.UIString("TEE"),
            4: WI.UIString("On Chip")
        };
        return protections[protection] || `Matcher ${protection}`;
    }

    _formatAuthenticatorDataSection(authData, section)
    {
        let ceremony = this._representedObject;
        let parsed = ceremony?.response?.parsedAuthenticatorData;

        if (!parsed) {
            console.error("Missing parsedAuthenticatorData from backend");
            this._appendKeyValuePairWithBase64Support(section, WI.UIString("Authenticator Data"), authData);
            return;
        }

        this._appendKeyValuePairWithBase64Support(section, WI.UIString("Authenticator Data (Raw)"), authData);

        let parsedSection = new WI.ResourceDetailsSection(WI.UIString("Parsed Authenticator Data"), "parsed-auth-data");
        
        let parentContainer = section.element.parentNode;
        if (parentContainer) {
            let nextSibling = section.element.nextSibling;
            if (nextSibling) {
                parentContainer.insertBefore(parsedSection.element, nextSibling);
            } else {
                parentContainer.appendChild(parsedSection.element);
            }
        } else {
            console.warn("Could not find parent container for parsed authenticator data section");
            return;
        }

        if (parsed.rpIdHash) {
            let rpIdHashElement = this._appendKeyValuePairWithBase64Support(parsedSection, WI.UIString("RP ID Hash"), parsed.rpIdHash);
        }
        
        let flagsText = [];
        if (parsed.flags.userPresent) flagsText.push(WI.UIString("User Present (UP)"));
        if (parsed.flags.rfu1) flagsText.push(WI.UIString("Reserved for Future Use 1 (RFU1)"));
        if (parsed.flags.userVerified) flagsText.push(WI.UIString("User Verified (UV)"));
        if (parsed.flags.backupEligible) flagsText.push(WI.UIString("Backup Eligible (BE)"));
        if (parsed.flags.backupState) flagsText.push(WI.UIString("Backup State (BS)"));
        if (parsed.flags.rfu2) flagsText.push(WI.UIString("Reserved for Future Use 2 (RFU2)"));
        if (parsed.flags.attestedCredentialDataIncluded) flagsText.push(WI.UIString("Attested Credential Data Included (AT)"));
        if (parsed.flags.extensionDataIncluded) flagsText.push(WI.UIString("Extension Data Included (ED)"));
        
        parsedSection.appendKeyValuePair(WI.UIString("Flags"), flagsText.length > 0 ? flagsText.join(", ") : WI.UIString("None"));

        parsedSection.appendKeyValuePair(WI.UIString("Signature Counter"), parsed.signCount.toLocaleString());

        // Attested Credential Data (if present)
        if (parsed.flags.attestedCredentialDataIncluded) {
            if (parsed.aaguid) {
                parsedSection.appendKeyValuePair(WI.UIString("AAGUID"), parsed.aaguid);
            }
            if (parsed.credentialId) {
                this._appendKeyValuePairWithBase64Support(parsedSection, WI.UIString("Credential ID"), parsed.credentialId);
            }
            if (parsed.credentialPublicKey) {
                let parsedKey = parsed.credentialPublicKey;

                // Create a subsection for the parsed public key
                let keySection = new WI.ResourceDetailsSection(WI.UIString("Credential Public Key"), "credential-public-key");

                let parentContainer = parsedSection.element.parentNode;
                if (parentContainer) {
                    let nextSibling = parsedSection.element.nextSibling;
                    if (nextSibling) {
                        parentContainer.insertBefore(keySection.element, nextSibling);
                    } else {
                        parentContainer.appendChild(keySection.element);
                    }
                }

                if (parsedKey.keyType !== undefined) {
                    let keyTypeStr = typeof parsedKey.keyType === "string" ? parsedKey.keyType : this._getCOSEKeyType(parsedKey.keyType);
                    keySection.appendKeyValuePair(WI.UIString("Key Type"), keyTypeStr);
                }
                if (parsedKey.algorithm !== undefined) {
                    let algStr = typeof parsedKey.algorithm === "string" ? parsedKey.algorithm : this._getCOSEAlgorithm(parsedKey.algorithm);
                    keySection.appendKeyValuePair(WI.UIString("Algorithm"), algStr);
                }
                if (parsedKey.curve !== undefined) {
                    let curveStr = typeof parsedKey.curve === "string" ? parsedKey.curve : this._getCOSECurve(parsedKey.curve);
                    keySection.appendKeyValuePair(WI.UIString("Curve"), curveStr);
                }
                if (parsedKey.keyId) keySection.appendKeyValuePair(WI.UIString("Key ID"), parsedKey.keyId);

                if (parsedKey.x) this._appendKeyValuePairWithBase64Support(keySection, WI.UIString("X Coordinate"), parsedKey.x, "x_coordinate");
                if (parsedKey.y) this._appendKeyValuePairWithBase64Support(keySection, WI.UIString("Y Coordinate"), parsedKey.y, "y_coordinate");
                if (parsedKey.d) this._appendKeyValuePairWithBase64Support(keySection, WI.UIString("D Value"), parsedKey.d, "d_value");
                if (parsedKey.n) this._appendKeyValuePairWithBase64Support(keySection, WI.UIString("Modulus"), parsedKey.n, "modulus");
                if (parsedKey.e) this._appendKeyValuePairWithBase64Support(keySection, WI.UIString("Exponent"), parsedKey.e, "exponent");
                if (parsedKey.p) this._appendKeyValuePairWithBase64Support(keySection, WI.UIString("P"), parsedKey.p, "p_value");
                if (parsedKey.q) this._appendKeyValuePairWithBase64Support(keySection, WI.UIString("Q"), parsedKey.q, "q_value");
                if (parsedKey.dP) this._appendKeyValuePairWithBase64Support(keySection, WI.UIString("dP"), parsedKey.dP, "dp_value");
                if (parsedKey.dQ) this._appendKeyValuePairWithBase64Support(keySection, WI.UIString("dQ"), parsedKey.dQ, "dq_value");
                if (parsedKey.qInv) this._appendKeyValuePairWithBase64Support(keySection, WI.UIString("qInv"), parsedKey.qInv, "qinv_value");
                if (parsedKey.keyValue) this._appendKeyValuePairWithBase64Support(keySection, WI.UIString("Key Value"), parsedKey.keyValue, "key_value");
                if (parsedKey.keyOps) keySection.appendKeyValuePair(WI.UIString("Key Operations"), Array.isArray(parsedKey.keyOps) ? parsedKey.keyOps.join(", ") : parsedKey.keyOps);
                if (parsedKey.baseIV) this._appendKeyValuePairWithBase64Support(keySection, WI.UIString("Base IV"), parsedKey.baseIV, "base_iv");
                if (parsedKey.lmsType) keySection.appendKeyValuePair(WI.UIString("LMS Type"), parsedKey.lmsType);
                if (parsedKey.lmotsType) keySection.appendKeyValuePair(WI.UIString("LM-OTS Type"), parsedKey.lmotsType);

                if (parsedKey.rawCBOR)
                    this._appendKeyValuePairWithBase64Support(keySection, WI.UIString("Public Key (Raw CBOR)"), parsedKey.rawCBOR, "raw_cbor");
            }
        }

        if (parsed.flags.extensionDataIncluded) {
            if (parsed.extensions && typeof parsed.extensions === "object") {
                for (const [extensionName, extensionValue] of Object.entries(parsed.extensions)) {
                    let displayValue = this._interpretWebAuthnExtension(extensionName, extensionValue);
                    parsedSection.appendKeyValuePair(WI.UIString("Extension: %s").format(extensionName), displayValue);
                }
            }

            if (parsed.extensionsRaw)
                this._appendKeyValuePairWithBase64Support(parsedSection, WI.UIString("Extensions (Raw CBOR)"), parsed.extensionsRaw);
        }
    }

    _createRequestContentView()
    {
        let ceremony = this._representedObject;
        let contentView = new WI.ContentView(null);
        contentView.element.classList.add("authentication-ceremony-request", "resource-details");

        // Request Overview Section
        let overviewSection = new WI.ResourceDetailsSection(WI.UIString("Request Overview"), "request-overview");
        contentView.element.appendChild(overviewSection.element);

        let request = ceremony?.request || {};

        if (request.rp) {
            overviewSection.appendKeyValuePair(WI.UIString("Relying Party ID"), request.rp.id || WI.UIString("Not specified"));
            if (request.rp.name)
                overviewSection.appendKeyValuePair(WI.UIString("Relying Party Name"), request.rp.name);
        }

        if (request.challenge) {
            let challengeValue = request.challenge;

            if (typeof challengeValue === "string") {
                this._appendKeyValuePairWithBase64Support(overviewSection, WI.UIString("Challenge"), challengeValue);
            } else if (challengeValue && typeof challengeValue === "object") {
                try {
                    let stringValue = String(challengeValue);
                    if (stringValue === "[object Object]") {
                        stringValue = JSON.stringify(challengeValue);
                    }
                    this._appendKeyValuePairWithBase64Support(overviewSection, WI.UIString("Challenge"), stringValue);
                } catch (e) {
                    console.error("Failed to convert request challenge object:", e);
                    this._appendKeyValuePairWithBase64Support(overviewSection, WI.UIString("Challenge"), "[Invalid Challenge Data]");
                }
            } else {
                this._appendKeyValuePairWithBase64Support(overviewSection, WI.UIString("Challenge"), String(challengeValue));
            }
        }

        if (request.timeout)
            overviewSection.appendKeyValuePair(WI.UIString("Timeout"), `${request.timeout} ms`);

        // User Information
        if (request.user) {
            let userSection = new WI.ResourceDetailsSection(WI.UIString("User Information"), "user-info");
            contentView.element.appendChild(userSection.element);

            if (request.user.id)
                this._appendKeyValuePairWithBase64Support(userSection, WI.UIString("User ID"), request.user.id);
            if (request.user.name)
                userSection.appendKeyValuePair(WI.UIString("User Name"), request.user.name);
            if (request.user.displayName)
                userSection.appendKeyValuePair(WI.UIString("Display Name"), request.user.displayName);
        }

        // Authenticator Selection
        if (request.authenticatorSelection) {
            let authSection = new WI.ResourceDetailsSection(WI.UIString("Authenticator Selection"), "auth-selection");
            contentView.element.appendChild(authSection.element);

            let selection = request.authenticatorSelection;
            if (selection.authenticatorAttachment) {
                let attachmentDisplay = selection.authenticatorAttachment === "platform" ? WI.UIString("Platform") : WI.UIString("Cross-Platform");
                authSection.appendKeyValuePair(WI.UIString("Attachment"), attachmentDisplay);
            }
            if (selection.userVerification)
                authSection.appendKeyValuePair(WI.UIString("User Verification"), selection.userVerification);
            if (selection.residentKey)
                authSection.appendKeyValuePair(WI.UIString("Resident Key"), selection.residentKey);
        }

        // Allow Credentials
        if (request.allowCredentials && request.allowCredentials.length > 0) {
            let credSection = new WI.ResourceDetailsSection(WI.UIString("Allowed Credentials"), "allowed-creds");
            contentView.element.appendChild(credSection.element);

            request.allowCredentials.forEach((cred, index) => {
                if (cred.id) {
                    // Convert base64url credential ID to hex for better readability
                    let hexCredentialId = this._base64ToHex(cred.id);
                    if (hexCredentialId) {
                        this._appendKeyValuePairWithBase64Support(credSection, WI.UIString("Credential %d ID").format(index + 1), hexCredentialId);
                    } else {
                        // Fallback to original if conversion fails
                        this._appendKeyValuePairWithBase64Support(credSection, WI.UIString("Credential %d ID").format(index + 1), cred.id);
                    }
                } else {
                    credSection.appendKeyValuePair(WI.UIString("Credential %d ID").format(index + 1), WI.UIString("Not specified"));
                }
                if (cred.type)
                    credSection.appendKeyValuePair(WI.UIString("Credential %d Type").format(index + 1), cred.type);
                if (cred.transports && cred.transports.length > 0)
                    credSection.appendKeyValuePair(WI.UIString("Credential %d Transports").format(index + 1), cred.transports.join(", "));
            });
        }

        return contentView;
    }

    _createResponseContentView()
    {
        let ceremony = this._representedObject;
        let contentView = new WI.ContentView(null);
        contentView.element.classList.add("authentication-ceremony-response", "resource-details");

        let response = ceremony?.response || {};

        // Response Overview Section
        let overviewSection = new WI.ResourceDetailsSection(WI.UIString("Response Overview"), "response-overview");
        contentView.element.appendChild(overviewSection.element);

        if (response.id) {
            // Convert base64url credential ID to hex for better readability
            let hexCredentialId = this._base64ToHex(response.id);
            if (hexCredentialId) {
                this._appendKeyValuePairWithBase64Support(overviewSection, WI.UIString("Credential ID"), hexCredentialId);
            } else {
                // Fallback to original if conversion fails
                this._appendKeyValuePairWithBase64Support(overviewSection, WI.UIString("Credential ID"), response.id);
            }
        }

        if (response.type)
            overviewSection.appendKeyValuePair(WI.UIString("Credential Type"), response.type);

        if (response.authenticatorAttachment) {
            let attachmentDisplay = response.authenticatorAttachment === "platform" ? WI.UIString("Platform") : WI.UIString("Cross-Platform");
            overviewSection.appendKeyValuePair(WI.UIString("Authenticator Attachment"), attachmentDisplay);
        }

        if (response.transports && response.transports.length > 0) {
            overviewSection.appendKeyValuePair(WI.UIString("Transports"), response.transports.join(", "));
        }

        // Authenticator Response Details
        if (response.response) {
            let authResponseSection = new WI.ResourceDetailsSection(WI.UIString("Authenticator Response"), "auth-response");
            contentView.element.appendChild(authResponseSection.element);

            let authResponse = response.response;

            if (authResponse.clientDataJSON)
                this._appendClientDataJSON(authResponseSection, authResponse.clientDataJSON);

            if (authResponse.authenticatorData)
                this._formatAuthenticatorDataSection(authResponse.authenticatorData, authResponseSection);

            if (authResponse.signature)
                this._appendKeyValuePairWithBase64Support(authResponseSection, WI.UIString("Signature"), authResponse.signature);

            if (authResponse.userHandle)
                this._appendKeyValuePairWithBase64Support(authResponseSection, WI.UIString("User Handle"), authResponse.userHandle);

            // For creation responses
            if (authResponse.attestationObject)
                this._appendKeyValuePairWithBase64Support(authResponseSection, WI.UIString("Attestation Object"), authResponse.attestationObject);

            if (authResponse.publicKey)
                this._appendKeyValuePairWithBase64Support(authResponseSection, WI.UIString("Public Key (raw)"), authResponse.publicKey, "public_key");

            if (authResponse.publicKeyAlgorithm) {
                let algName = this._getAlgorithmName(authResponse.publicKeyAlgorithm);
                authResponseSection.appendKeyValuePair(WI.UIString("Public Key Algorithm"), `${algName} (${authResponse.publicKeyAlgorithm})`);
            }
        }

        // Client Extensions
        if (response.clientExtensionResults && Object.keys(response.clientExtensionResults).length > 0) {
            let extensionsSection = new WI.ResourceDetailsSection(WI.UIString("Client Extension Results"), "client-extensions");
            contentView.element.appendChild(extensionsSection.element);

            // Show extensions as collapsible JSON if there are multiple or complex extensions
            if (Object.keys(response.clientExtensionResults).length > 1 ||
                Object.values(response.clientExtensionResults).some(value => typeof value === "object" && value !== null)) {
                let extensionsContainer = JSON.stringify(response.clientExtensionResults, null, 2);
                extensionsSection.appendKeyValuePair(WI.UIString("All Extension Results"), extensionsContainer);
            } else {
                // For simple single extensions, show them individually
                for (let [key, value] of Object.entries(response.clientExtensionResults))
                    extensionsSection.appendKeyValuePair(key, JSON.stringify(value));
            }
        }

        return contentView;
    }

    _appendClientDataJSON(section, clientDataJSONBase64)
    {
        try {
            if (!clientDataJSONBase64 || typeof clientDataJSONBase64 !== "string") {
                throw new Error("Invalid clientDataJSON input");
            }
            
            const jsonString = this._base64urlDecode(clientDataJSONBase64);
            
            if (!jsonString || jsonString.length === 0) {
                throw new Error("Failed to decode base64url data");
            }
            
            const parsedJSON = JSON.parse(jsonString);

            if (!parsedJSON || typeof parsedJSON !== "object") {
                throw new Error("Decoded data is not a valid JSON object");
            }

            // Protect against prototype pollution
            delete parsedJSON.__proto__;
            delete parsedJSON.constructor;
            delete parsedJSON.prototype;
            
            let formattedJSON = JSON.stringify(parsedJSON, null, 2);
            let valueElement = section.appendKeyValuePair(WI.UIString("Client Data JSON"), formattedJSON);

            valueElement.addEventListener("contextmenu", (event) => {
                let contextMenu = WI.ContextMenu.createFromEvent(event);

                contextMenu.appendItem(WI.UIString("Copy as JSON"), () => {
                    InspectorFrontendHost.copyText(formattedJSON);
                });

                contextMenu.appendItem(WI.UIString("Copy as Base64URL"), () => {
                    InspectorFrontendHost.copyText(clientDataJSONBase64);
                });
            });

            valueElement.style.cursor = "context-menu";
            valueElement.title = WI.UIString("Right-click to copy in different formats");
        } catch (e) {
            console.error("Failed to parse Client Data JSON:", e);
            this._appendKeyValuePairWithBase64Support(section, WI.UIString("Client Data JSON"), clientDataJSONBase64);
        }
    }

    _base64urlDecode(base64urlStr)
    {
        try {
            let base64 = this._normalizeBase64url(base64urlStr);

            const binaryString = atob(base64);

            return binaryString;
        } catch (e) {
            console.error("Failed to decode base64url:", e);
            return null;
        }
    }

    _isBase64urlString(str)
    {
        if (!str || typeof str !== "string")
            return false;
        
        const base64urlPattern = /^[A-Za-z0-9_-]+$/;
        return base64urlPattern.test(str) && str.length > 8; // Minimum reasonable length
    }
};
