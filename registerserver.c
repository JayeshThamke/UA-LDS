/******************************************************************************
**
** Copyright (C) 2005-2013 Unified Automation GmbH. All Rights Reserved.
** Web: http://www.unifiedautomation.com
**
** Project: OPC UA Local Discovery Server
**
** Author: Gerhard Gappmeier <gerhard.gappmeier@ascolab.com>
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
******************************************************************************/

/**
 * \addtogroup ualds OPC UA LDS Module
 * @{
 */

/* system includes */
#include <time.h>
/* uastack includes */
#include <opcua_serverstub.h>
#include <opcua_string.h>
#include <opcua_datetime.h>
#include <opcua_memory.h>
/* local includes */
#include "config.h"
#include "settings.h"
#include "ualds.h"
/* local platform includes */
#include <platform.h>
#include <log.h>

/** RegisterServer Service implementation.
* @param hEndpoint OPC UA Endpoint handle.
* @param hContext  Service context.
* @param ppRequest Pointer to decoded service request structure.
* @param pRequestType The service type.
*
* @return OpcUa_StatusCode
*/
OpcUa_StatusCode ualds_registerserver(
    OpcUa_Endpoint        hEndpoint,
    OpcUa_Handle          hContext,
    OpcUa_Void          **ppRequest,
    OpcUa_EncodeableType *pRequestType)
{
    OpcUa_RegisterServerRequest  *pRequest;
    OpcUa_RegisterServerResponse *pResponse;
    OpcUa_EncodeableType         *pResponseType = 0;
    OpcUa_StatusCode              uStatus = OpcUa_Good;
    int i;
    int numServers = 0;
    int bExists = 0;
    char szServerUri[UALDS_CONF_MAX_URI_LENGTH];
    char *pszServerUri;

    UALDS_UNUSED(pRequestType);

    OpcUa_ReturnErrorIfArgumentNull(ppRequest);
    pRequest = *ppRequest;

    uStatus = OpcUa_Endpoint_BeginSendResponse(
        hEndpoint,
        hContext,
        (OpcUa_Void**)&pResponse,
        &pResponseType);
    OpcUa_ReturnErrorIfBad(uStatus);

    if (pResponse)
    {
        OpcUa_Boolean bIsOnline = OpcUa_False;

        if (OpcUa_IsGood(uStatus))
        {
            /* check for valid parameters */
            if (OpcUa_String_StrLen(&pRequest->Server.ServerUri) == 0 ||
                ( OpcUa_String_StrnCmp(&pRequest->Server.ServerUri, OpcUa_String_FromCString("urn:"), 4, OpcUa_False) != 0 &&
                  OpcUa_String_StrnCmp(&pRequest->Server.ServerUri, OpcUa_String_FromCString("http://"), 7, OpcUa_False) != 0))
            {
                ualds_log(UALDS_LOG_ERR, "ServerUri '%s' is invalid (needs to start with 'urn:' or 'http://').", OpcUa_String_GetRawString(&pRequest->Server.ServerUri));
                uStatus = OpcUa_BadServerUriInvalid;
            }
            else if (pRequest->Server.NoOfServerNames <= 0 ||
                OpcUa_String_StrLen(&pRequest->Server.ServerNames[0].Text) == 0)
            {
                ualds_log(UALDS_LOG_ERR, "No ServerNames have been provided.");
                uStatus = OpcUa_BadServerNameMissing;
            }
            else if (pRequest->Server.NoOfDiscoveryUrls <= 0 ||
                OpcUa_String_StrLen(&pRequest->Server.DiscoveryUrls[0]) == 0)
            {
                ualds_log(UALDS_LOG_ERR, "No DiscoveryUrls have been provided.");
                uStatus = OpcUa_BadDiscoveryUrlMissing;
            }
        }

        if (OpcUa_IsGood(uStatus))
        {
            switch (pRequest->Server.ServerType)
            {
            case OpcUa_ApplicationType_Server:
            case OpcUa_ApplicationType_ClientAndServer:
            case OpcUa_ApplicationType_DiscoveryServer:
                break;
            default:
                ualds_log(UALDS_LOG_ERR, "Invalid ServerType %i.", pRequest->Server.ServerType);
                uStatus = OpcUa_BadInvalidArgument;
                break;
            }
        }

        if (OpcUa_IsGood(uStatus))
        {
            /* get pointer to the request's server uri, this makes coding easier */
            pszServerUri = OpcUa_String_GetRawString(&pRequest->Server.ServerUri);

            /* ignore IsOnline if SemaphoreFilePath is set */
            if (OpcUa_String_StrLen(&pRequest->Server.SemaphoreFilePath) > 0)
            {
                FILE *f = fopen(OpcUa_String_GetRawString(&pRequest->Server.SemaphoreFilePath), "r");
                if (f)
                {
                    /* file exists */
                    bIsOnline = OpcUa_True;
                    fclose(f);
                }
                else
                {
                    uStatus = OpcUa_BadSempahoreFileMissing;
                }
            }
            else
            {
                bIsOnline = pRequest->Server.IsOnline;
            }

            if (bIsOnline != OpcUa_False)
            {
                ualds_log(UALDS_LOG_INFO, "Registering server %s.", pszServerUri);
                ualds_settings_begingroup("RegisteredServers");
                ualds_settings_beginreadarray("Servers", &numServers);
                /* check if requested server is already registered */
                for (i=0; i<numServers; i++)
                {
                    ualds_settings_setarrayindex(i);
                    ualds_settings_readstring("ServerUri", szServerUri, sizeof(szServerUri));
                    if (strcmp(szServerUri, pszServerUri) == 0)
                    {
                        bExists = 1;
                        break;
                    }
                }
                ualds_settings_endarray();
                if (bExists == 0)
                {
                    /* add new server */
                    ualds_settings_beginwritearray("Servers", numServers+1);
                    ualds_settings_setarrayindex(numServers);
                    ualds_settings_writestring("ServerUri", pszServerUri);
                    ualds_settings_endarray();
                }
                ualds_settings_endgroup();

                /* update server info */
                ualds_settings_begingroup(pszServerUri);
                ualds_settings_writestring("ProductUri", OpcUa_String_GetRawString(&pRequest->Server.ProductUri));
                ualds_settings_beginwritearray("ServerNames", pRequest->Server.NoOfServerNames);
                for (i=0; i<pRequest->Server.NoOfServerNames; i++)
                {
                    ualds_settings_setarrayindex(i);
                    ualds_settings_writestring("Locale", OpcUa_String_GetRawString(&pRequest->Server.ServerNames[i].Locale));
                    ualds_settings_writestring("Text", OpcUa_String_GetRawString(&pRequest->Server.ServerNames[i].Text));
                }
                ualds_settings_endarray();
                ualds_settings_writeint("ServerType", pRequest->Server.ServerType);
                ualds_settings_writestring("GatewayServerUri", OpcUa_String_GetRawString(&pRequest->Server.GatewayServerUri));
                ualds_settings_beginwritearray("DiscoveryUrls", pRequest->Server.NoOfDiscoveryUrls);
                for (i=0; i<pRequest->Server.NoOfDiscoveryUrls; i++)
                {
                    ualds_settings_setarrayindex(i);
                    ualds_settings_writestring("Url", OpcUa_String_GetRawString(&pRequest->Server.DiscoveryUrls[i]));
                }
                ualds_settings_endarray();
                ualds_settings_writestring("SemaphoreFilePath", OpcUa_String_GetRawString(&pRequest->Server.SemaphoreFilePath));
                ualds_settings_writetime_t("UpdateTime", time(0));
                ualds_settings_endgroup();
                ualds_settings_flush();
            }
            else
            {
                /* unregister */
                ualds_log(UALDS_LOG_INFO, "Unregistering server %s.", pszServerUri);
                ualds_settings_removegroup(pszServerUri);
                ualds_settings_flush();
                ualds_expirationcheck();
            }
        }

        UALDS_BUILDRESPONSEHEADER;

        /* Send response */
        OpcUa_Endpoint_EndSendResponse(
            hEndpoint,
            &hContext,
            uStatus,
            pResponse,
            pResponseType);

        /* free response */
        OpcUa_RegisterServerResponse_Clear(pResponse);
        OpcUa_Free(pResponse);

        /* we have send a response, either with good or bad status, this does not matter.
         * we must return Good now, otherwise the stack will send an error message.
         */
        uStatus = OpcUa_Good;
    }

    return uStatus;
}

/** @} */
