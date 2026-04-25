// Internet Explorer Test Pages Copyright © 2012 Microsoft Corporation. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
//
// Redistributions of source code must retain the above copyright notice, this list of
// conditions and the following disclaimer.
//
// Redistributions in binary form must reproduce the above copyright notice, this list of
// conditions and the following disclaimer in the documentation and/or other materials
// provided with the distribution.
//
// Neither the name of the Microsoft Corporation nor the names of its contributors may be
// used to endorse or promote products derived from this software without specific prior
// written permission.
//
// THIS SOFTWARE IS PROVIDED BY MICROSOFT CORPORATION "AS IS" AND ANY EXPRESS OR IMPLIED
// WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
// FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MICROSOFT CORPORATION
// BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
// OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.


// This function returns the value of an API feature if it is defined with one of
// the known ventor prefixes.
//
// Parameters:
//  parent: the object containing the API feature
//  feature: the name of the API feature, this should be a string value
//  isAttribute: set this to true to indicate that this is a constant attribute
//               as opposed to a variable
function BrowserHasFeature(parent, feature, isAttribute)
{
    if (parent[feature] !== undefined)
    {
        //feature is defined without a vendor prefix, no further checks necessary
        return parent[feature];
    }

    // the feature is not defined without a vendor prefix, so find the vendor prefix, if any,
    // that it is defined with
    var prefix = GetVendorPrefix(parent, feature, isAttribute);

    // if prefix is not undefined, then the feature has been found to exist with this prefix
    if (prefix !== undefined)
    {
        var prefixedFeatureName = AppendPrefix(prefix, feature, isAttribute);
        return parent[prefixedFeatureName];
    }

    //The feature does not exist.
    //Callers should check for !==undefined as the feature itself could return
    //a Bool which would fail a typical if(feature) check
    return undefined;
}

// This function returns the vendor prefix found if a certain feature is defined with it.
// It takes the same parameters at BrowserHasFeature().
function GetVendorPrefix(parent, feature, isAttribute)
{
    //Known vendor prefixes
    var VendorPrefixes = ["moz", "ms", "o", "webkit"];
    for (vendor in VendorPrefixes)
    {
        //Build up the new feature name with the vendor prefix
        var prefixedFeatureName = AppendPrefix(VendorPrefixes[vendor], feature, isAttribute);
        if (parent[prefixedFeatureName] !== undefined)
        {
            //Vendor prefix version exists, return a pointer to the feature
            return VendorPrefixes[vendor];
        }
    }

    // if no version of the feature with a vendor prefix has been found, return undefined
    return undefined;
}

// This will properly capitalize the feature name and then return the feature name preceded
// with the provided vendor prefix. If the prefix given is undefined, this function will
// return the feature name given as is. The output of this function should not be used
// as an indicator of whether or not a feature exists as it will return the same thing if
// the inputted feature is undefined or is defined without a vendor prefix. It takes the
// same parameters at BrowserHasFeature().
function AppendPrefix(prefix, feature, isAttribute)
{
    if (prefix !== undefined)
    {
        if (isAttribute)
        {
            // if a certain feature is an attribute, then it follows a different naming standard
            // where it must be completely capitalized and have words split with an underscore
            return prefix.toUpperCase() + "_" + feature.toUpperCase();
        }
        else
        {
            //Vendor prefixing should follow the standard convention: vendorprefixFeature
            //Note that the feature is capitalized after the vendor prefix
            //Therefore if the feature is window.feature, the vendor prefix version is:
            //window.[vp]Feature where vp is the vendor prefix:
            //window.msFeature, window.webkitFeature, window.mozFeature, window.oFeature
            var newFeature = feature[0].toUpperCase() + feature.substr(1, feature.length);

            //Build up the new feature name with the vendor prefix
            return prefix + newFeature;
        }
    }
    else
    {
        return feature;
    }
}
