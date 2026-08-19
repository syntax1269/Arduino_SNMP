#include "include/defs.h"
#include "include/SNMPParser.h"
#include "include/BER.h"
#include "include/ValueCallbacks.h"

template<typename... Args>
static inline bool appendResponseVarBind(VarBind out[], int &outCount, Args&&... args){
    if(outCount >= SNMP_MAX_VARBINDS) return false;
    out[outCount].~VarBind();
    new (&out[outCount]) VarBind(std::forward<Args>(args)...);
    outCount++;
    return true;
}

bool handleGetRequestPDU(ValueCallback* const *callbacks, int callbacksCount, const VarBind* varbindList, int varbindCount, VarBind outResponseList[], int &outResponseCount, SNMP_VERSION snmpVersion, bool isGetNextRequest){
    SNMP_LOGD("handleGetRequestPDU\n");
    for(int vbIdx = 0; vbIdx < varbindCount; vbIdx++){
        const VarBind& requestVarBind = varbindList[vbIdx];
        SNMP_LOGD("finding callback for OID: %s\n", requestVarBind.oid->string());
        ValueCallback* callback = ValueCallback::findCallback(callbacks, callbacksCount, requestVarBind.oid, isGetNextRequest);
        if(!callback){
            SNMP_LOGD("Couldn't find callback\n");
#if 1
            if(isGetNextRequest){
                appendResponseVarBind(outResponseList, outResponseCount, requestVarBind, asn_new<ImplicitNullType>(ENDOFMIBVIEW));
            } else {
                appendResponseVarBind(outResponseList, outResponseCount, requestVarBind, asn_new<ImplicitNullType>(NOSUCHOBJECT));
            }

#else
            appendResponseVarBind(outResponseList, outResponseCount, generateErrorResponse(SNMP_ERROR_VERSION_CTRL_DEF(NOT_WRITABLE, snmpVersion, NO_SUCH_NAME), requestVarBind.oid));
#endif
            continue;
        }

        SNMP_LOGD("Callback found with OID: %s\n", callback->OID->string());
        auto value = ValueCallback::getValueForCallback(callback);

        if(!value){
            SNMP_LOGD("Couldn't get value for callback\n");
            appendResponseVarBind(outResponseList, outResponseCount, callback->OID, SNMP_ERROR_VERSION_CTRL(GEN_ERR, snmpVersion));
            continue;
        }

        appendResponseVarBind(outResponseList, outResponseCount, callback->OID, value);
    }
    return true;
}

bool handleSetRequestPDU(ValueCallback* const *callbacks, int callbacksCount, const VarBind* varbindList, int varbindCount, VarBind outResponseList[], int &outResponseCount, SNMP_VERSION snmpVersion){
    SNMP_LOGD("handleSetRequestPDU\n");
    for(int vbIdx = 0; vbIdx < varbindCount; vbIdx++){
        const VarBind& requestVarBind = varbindList[vbIdx];
        SNMP_LOGD("finding callback for OID: %s\n", requestVarBind.oid->string());
        ValueCallback* callback = ValueCallback::findCallback(callbacks, callbacksCount, requestVarBind.oid, false);
        if(!callback){
            SNMP_LOGD("Couldn't find callback\n");
            appendResponseVarBind(outResponseList, outResponseCount, requestVarBind.oid->cloneRaw(), SNMP_ERROR_VERSION_CTRL_DEF(NOT_WRITABLE, snmpVersion, NO_SUCH_NAME));
            continue;
        }

        SNMP_LOGD("Callback found with OID: %s\n", callback->OID->string());

        if(callback->type != requestVarBind.type){
            SNMP_LOGD("Callback Type mismatch: %d\n", callback->type);
            appendResponseVarBind(outResponseList, outResponseCount, requestVarBind.oid->cloneRaw(), SNMP_ERROR_VERSION_CTRL_DEF(WRONG_TYPE, snmpVersion, BAD_VALUE));
            continue;
        }

        if(!callback->isSettable){
            SNMP_LOGD("Cannot set this object\n");
            appendResponseVarBind(outResponseList, outResponseCount, requestVarBind.oid->cloneRaw(), SNMP_ERROR_VERSION_CTRL(READ_ONLY, snmpVersion));
            continue;
        }
        std::shared_ptr<BER_CONTAINER> valueView(requestVarBind.value, [](BER_CONTAINER*){});
        SNMP_ERROR_STATUS setError = ValueCallback::setValueForCallback(callback, valueView);
        if(setError != NO_ERROR){
            SNMP_LOGD("Attempting to set Variable failed: %d\n", setError);
            appendResponseVarBind(outResponseList, outResponseCount, callback->OID, SNMP_ERROR_VERSION_CTRL(setError, snmpVersion));
            continue;
        }

        auto value = ValueCallback::getValueForCallback(callback);

        if(!value){
            SNMP_LOGD("Couldn't get value for callback\n");
            appendResponseVarBind(outResponseList, outResponseCount, callback->OID, SNMP_ERROR_VERSION_CTRL(GEN_ERR, snmpVersion));
            continue;
        }

        appendResponseVarBind(outResponseList, outResponseCount, callback->OID, value);
    }
    return true;

}

bool handleGetBulkRequestPDU(ValueCallback* const *callbacks, int callbacksCount, const VarBind* varbindList, int varbindCount, VarBind outResponseList[], int &outResponseCount, unsigned int nonRepeaters, unsigned int maxRepititions){
    SNMP_LOGD("handleGetBulkRequestPDU, nonRepeaters:%d, maxRepititions:%d, varbindSize:%d\n", nonRepeaters, maxRepititions, varbindCount);

    SNMP_LOGD("handling nonRepeaters\n");
    if(nonRepeaters > 0){
        unsigned int bound = (nonRepeaters < (unsigned int)varbindCount) ? nonRepeaters : (unsigned int)varbindCount;
        for(unsigned int i = 0; i < bound; i++){
            const VarBind& requestVarBind = varbindList[i];
            ValueCallback* callback = ValueCallback::findCallback(callbacks, callbacksCount, requestVarBind.oid, true);
            if(!callback){
                appendResponseVarBind(outResponseList, outResponseCount, requestVarBind, asn_new<ImplicitNullType>(ENDOFMIBVIEW));
                continue;
            }

            auto value = ValueCallback::getValueForCallback(callback);
            if(!value){
                SNMP_LOGD("Couldn't get value for callback\n");
                appendResponseVarBind(outResponseList, outResponseCount, callback->OID, GEN_ERR);
                continue;
            }
            appendResponseVarBind(outResponseList, outResponseCount, requestVarBind, value);
        }
    }

    if(varbindCount > (int)nonRepeaters){
        SNMP_LOGD("handling repeaters\n");
        unsigned int repeatingVarBinds = varbindCount - nonRepeaters;

        for(unsigned int i = 0; i < repeatingVarBinds; i++){
            OIDType* oid = varbindList[i+nonRepeaters].oid->cloneRaw();
            int foundAt = 0;

            for(unsigned int j = 0; j < maxRepititions; j++){
                SNMP_LOGD("finding next callback for OID: %s\n", oid->string());
                ValueCallback* callback = ValueCallback::findCallback(callbacks, callbacksCount, oid, true, foundAt, &foundAt);
                if(!callback){
                    appendResponseVarBind(outResponseList, outResponseCount, oid, asn_new<ImplicitNullType>(ENDOFMIBVIEW));
                    oid = nullptr;
                    break;
                }

                auto value = ValueCallback::getValueForCallback(callback);

                if(!value){
                    SNMP_LOGD("Couldn't get value for callback\n");
                    appendResponseVarBind(outResponseList, outResponseCount, callback->OID, GEN_ERR);
                    break;
                }

                appendResponseVarBind(outResponseList, outResponseCount, callback->OID, value);

                asn_delete(oid);
                oid = callback->OID->cloneRaw();
            }

            asn_delete(oid);
        }
    }

    return true;
}