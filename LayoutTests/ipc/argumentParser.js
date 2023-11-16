let knownSizes = {
    "unsigned": 4,
    "int": 4,
    "uint8_t": 1,
    "int8_t": 1,
    "uint64_t": 8,
    "int64_t": 8,
    "uint32_t": 4,
    "int32_t": 4,
    "uint16_t": 2,
    "int16_t": 2,
    "double": 8,
    "float": 4,
    "bool": 1,
    "size_t": 8,
    "unsigned short": 2,
    "long long": 8,
    "UUID": 16
};


// manual additions
$F.fuzzerTypeInfo = {};
$F.fuzzerTypeInfo['WebCore::DOMCacheEngine::CacheIdentifierOrError'] = [{'type': 'Expected<WebCore::DOMCacheEngine::CacheIdentifierOperationResult, Error>'}];

let ArgumentParser = {};
function align(pos, granularity) {
    if(pos%granularity) {
        pos = pos + granularity-(pos%granularity);
    }
    return pos;
}

ArgumentParser.parseMessage = function(msg, proc) {
    let buffer = new Uint8Array(msg.buffer);
    window.bada = buffer;
    let name = msg.description;
    let args = IPCmessages[name].arguments;
    let results = {};
    try {
        ArgumentParser.parse(buffer, args, 0x10 /*skip message header*/, results);
        if(proc) results[proc + "Connection"] = [ msg.destinationID ];
        let clz = msg.description.substring(0, msg.description.indexOf("_"));
        if(proc) results["clz_" + clz + "Connection"] = [ msg.destinationID ];
        //$F.exec.log("//blam: " + msg.destinationID);
        if($F.rand && $F.rand.chance(16)) {
            //$F.exec.log("// focus on " + clz);
            $F.focusClass = clz;
        }
    } catch (e) {
        //$F.exec.log("//Bug: " + e);
    }
    return results;
}

ArgumentParser.parseMessageFromReply = function(args, buffer) {
    buffer = new Uint8Array(buffer);
    let results = {};
    try {
        ArgumentParser.parse(buffer, args, 0x10/*skip message header*/, results);
    } catch (e) {
        //$F.exec.log("//Bug: " + e);
    }
    return results;
}

function addResult(results, t, id) {
    if(t in results) {
        results[t].push(id);
    } else {
        results[t] = [ id ];
    }
}

ArgumentParser.parse = function(buf, args, pos, results) {
    if(args == null) return -1;
    if(args.length == 0) return pos;
    let arg = args.shift();
    let t = arg['type'];
    //$F.exec.log("// next " + t + " -> " + arg['optional']);
    if(arg['optional']) {
        let haz = !!buf[pos];
        pos += 1
        if(!haz) {
            //$F.exec.log("// skipping optional argument")
            return ArgumentParser.parse(buf, args, pos, results);
        }
    }
    if (processQualified.includes(t)) {
        pos = align(pos, 8);
        if(pos + 16 > buf.length) {
            //$F.exec.log("// Message too small to contain identifier")
            return -1;
        }
        let view = new DataView(buf.buffer, pos);
        let id1 = view.getBigUint64(0, true);
        let id2 = view.getBigUint64(8, true);
        addResult(results, t, id1);
        addResult(results, "WebCore::ProcessIdentifier", id2);
        return ArgumentParser.parse(buf, args, pos + 16, results);
    } else if ( IPCobjectIdentifiers.includes(t) ) {
        pos = align(pos, 8);
        if(pos + 8 > buf.length) {
            //$F.exec.log("// Message too small to contain identifier")
            return -1;
        }
        let view = new DataView(buf.buffer, pos);
        let id = view.getBigUint64(0, true);
        //$F.exec.log("// Got Identifier '" + t + "': " + id);
        //$F.objstore.addObj(t, id);
        addResult(results, t, id);
        return ArgumentParser.parse(buf, args, pos + 8, results);
    } else if ( knownSizes[t] ) {
        pos = align(pos, knownSizes[t]);
        pos += knownSizes[t];
        return ArgumentParser.parse(buf, args, pos, results);
    } else if ( IPCserializedEnumInfo[t] ) {
        let enumInfo = IPCserializedEnumInfo[t];
        pos = align(pos, enumInfo.size);
        pos += enumInfo.size;
        return ArgumentParser.parse(buf, args, pos, results);
    } else if ( $F.fuzzerTypeInfo[t] ) {
        pos = ArgumentParser.parse(buf, $F.fuzzerTypeInfo[t], pos, results);
        if(pos==-1) return -1;
        return ArgumentParser.parse(buf, args, pos, results);
    } else if ( IPCserializedTypeInfo[t] ) {
        let typeInfo = IPCserializedTypeInfo[t];
        let subArgs = [];
        for (const element of typeInfo) {
            subArgs.push(element);
        }
        pos = ArgumentParser.parse(buf, subArgs, pos, results);
        if(pos==-1) return -1;
        return ArgumentParser.parse(buf, args, pos, results);
    } else if ( t == "String" || t == "URL" ) {
        pos = align(pos, 4);
        let view = new DataView(buf.buffer, pos);
        let strLen = view.getUint32(0, true);
        pos += 4;
        if ( buf[pos++] == 0x01 ) {
            let str = "";
            for(let i=0; i<strLen; i++) {
                str += String.fromCharCode(buf[pos++]);
            }
            return ArgumentParser.parse(buf, args, pos, results);
        }  else {
            // Other encodings are not supported
            return -1;
        }
    } else if ( t.indexOf("std::pair<") == 0) {
        let types = t.substr(10).split(", ");
        for(const nextt of types) {
            let contentArg = {'type': nextt};
            pos = ArgumentParser.parse(buf, [ contentArg ], pos, results);
            if(pos==-1) return -1;
        }
        return ArgumentParser.parse(buf, args, pos, results);
    } else if ( t.indexOf("Expected<") == 0) {
        let haz = !!buf[pos];
        pos += 1
        if(haz) {
            let expected_type = t.substr(9).split(",")[0];
            let contentArg = {'type': expected_type};
            pos = ArgumentParser.parse(buf, [ contentArg ], pos, results);
            if (pos == -1) {
                return -1;
            }
            return ArgumentParser.parse(buf, args, pos, results);
        } else {
            return -1;
        }
    } else if ( t.indexOf("Vector<") == 0 ) {
        pos = align(pos, 8);
        if(pos + 8 > buf.length) {
            //$F.exec.log("// Message too small to contain identifier")
            return -1;
        }
        let view = new DataView(buf.buffer, pos);
        let id = view.getBigUint64(0, true);
        pos += 8;
        if(id > 0x1000) {
            return -1;
        }
        let contentArg = {'type': t.substring(7, t.length-1)};
        for(let x = 0; x<id; x++) {
            pos = ArgumentParser.parse(buf, [ contentArg ], pos, results);
            if (pos == -1) {
                return -1
            }
        }
        return ArgumentParser.parse(buf, args, pos, results);
    } else {
        //$F.exec.log("//[x] could not parse '"+t+"'");
    }
    return -1;
}
