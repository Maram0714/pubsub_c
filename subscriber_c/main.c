/* OPC UA PubSub Subscriber - Heat Pump side
 * Receives power limit command {PLimit:4200.00} from DSO Publisher
 * Matches publisher configuration: PublisherId=2234, WriterGroupId=100, DataSetWriterId=62541
 */
#include <open62541/server_config_default.h>
#include <open62541/plugin/log_stdout.h>
#include <open62541/server.h>
#include <open62541/server_pubsub.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static UA_NodeId connectionIdentifier;
static UA_NodeId readerGroupIdentifier;
static UA_NodeId readerIdentifier;
static UA_DataSetReaderConfig readerConfig;

/* Target variable node where received value is stored */
static UA_NodeId powerLimitTargetNode;

/* --------------------------------------------------
 * Add PubSub Connection
 * -------------------------------------------------- */
static void
addPubSubConnection(UA_Server *server, UA_String *transportProfile,
                    UA_NetworkAddressUrlDataType *networkAddressUrl) {
    UA_PubSubConnectionConfig connectionConfig;
    memset(&connectionConfig, 0, sizeof(UA_PubSubConnectionConfig));
    connectionConfig.name = UA_STRING("HP Subscriber Connection");
    connectionConfig.transportProfileUri = *transportProfile;
    UA_Variant_setScalar(&connectionConfig.address, networkAddressUrl,
                         &UA_TYPES[UA_TYPES_NETWORKADDRESSURLDATATYPE]);
    connectionConfig.publisherId.idType = UA_PUBLISHERIDTYPE_UINT32;
    connectionConfig.publisherId.id.uint32 = UA_UInt32_random();
    UA_Server_addPubSubConnection(server, &connectionConfig, &connectionIdentifier);
}

/* --------------------------------------------------
 * Add Reader Group
 * -------------------------------------------------- */
static void addReaderGroup(UA_Server *server) {
    UA_ReaderGroupConfig readerGroupConfig;
    memset(&readerGroupConfig, 0, sizeof(UA_ReaderGroupConfig));
    readerGroupConfig.name = UA_STRING("HP ReaderGroup");
    UA_Server_addReaderGroup(server, connectionIdentifier,
                             &readerGroupConfig, &readerGroupIdentifier);
}

/* --------------------------------------------------
 * Fill DataSet MetaData - must match publisher fields exactly
 * -------------------------------------------------- */
static void fillDataSetMetaData(UA_DataSetMetaDataType *pMetaData) {
    UA_DataSetMetaDataType_init(pMetaData);
    pMetaData->name = UA_STRING("DSO Power Commands");

    /* One field: PowerLimit as String */
    pMetaData->fieldsSize = 1;
    pMetaData->fields = (UA_FieldMetaData *)UA_Array_new(
        pMetaData->fieldsSize, &UA_TYPES[UA_TYPES_FIELDMETADATA]);

    UA_FieldMetaData_init(&pMetaData->fields[0]);
    UA_NodeId_copy(&UA_TYPES[UA_TYPES_STRING].typeId,
                   &pMetaData->fields[0].dataType);
    pMetaData->fields[0].builtInType = UA_NS0ID_STRING;
    pMetaData->fields[0].name = UA_STRING("PowerLimit");
    pMetaData->fields[0].valueRank = -1; /* scalar */
}

/* --------------------------------------------------
 * Add DataSet Reader
 * -------------------------------------------------- */
static void addDataSetReader(UA_Server *server) {
    memset(&readerConfig, 0, sizeof(UA_DataSetReaderConfig));
    readerConfig.name = UA_STRING("HP DataSet Reader");

    /* Must match publisher exactly */
    readerConfig.publisherId.idType = UA_PUBLISHERIDTYPE_UINT16;
    readerConfig.publisherId.id.uint16 = 2234;   /* matches publisher */
    readerConfig.writerGroupId   = 100;           /* matches publisher */
    readerConfig.dataSetWriterId = 62541;         /* matches publisher */

    fillDataSetMetaData(&readerConfig.dataSetMetaData);

    UA_Server_addDataSetReader(server, readerGroupIdentifier,
                               &readerConfig, &readerIdentifier);
}

/* --------------------------------------------------
 * Add Subscribed Variables (Target Variables)
 * -------------------------------------------------- */
static void addSubscribedVariables(UA_Server *server, UA_NodeId dataSetReaderId) {
    /* Create folder node */
    UA_NodeId folderId;
    UA_ObjectAttributes oAttr = UA_ObjectAttributes_default;
    oAttr.displayName = UA_LOCALIZEDTEXT("en-US", "HP Subscribed Variables");
    UA_Server_addObjectNode(server, UA_NODEID_NULL,
                            UA_NS0ID(OBJECTSFOLDER), UA_NS0ID(ORGANIZES),
                            UA_QUALIFIEDNAME(1, "HP Subscribed Variables"),
                            UA_NS0ID(BASEOBJECTTYPE), oAttr, NULL, &folderId);

    /* Create target variable for PowerLimit string */
    UA_VariableAttributes vAttr = UA_VariableAttributes_default;
    vAttr.displayName = UA_LOCALIZEDTEXT("en-US", "PowerLimit");
    vAttr.dataType = UA_TYPES[UA_TYPES_STRING].typeId;
    UA_String initVal = UA_STRING("");
    UA_Variant_setScalar(&vAttr.value, &initVal, &UA_TYPES[UA_TYPES_STRING]);

UA_NodeId newNode;
UA_Server_addVariableNode(server,
                          UA_NODEID_STRING(1, "power.limit.sub"),
                          folderId,
                          UA_NS0ID(HASCOMPONENT),
                          UA_QUALIFIEDNAME(1, "PowerLimit"),
                          UA_NS0ID(BASEDATAVARIABLETYPE),
                          vAttr, NULL, &powerLimitTargetNode);

    /* Link target variable to DataSetReader */
    UA_FieldTargetDataType targetVar;
    UA_FieldTargetDataType_init(&targetVar);
    targetVar.attributeId  = UA_ATTRIBUTEID_VALUE;
    targetVar.targetNodeId = powerLimitTargetNode;

    UA_Server_DataSetReader_createTargetVariables(server, dataSetReaderId, 1, &targetVar);
    UA_free(readerConfig.dataSetMetaData.fields);
}

/* --------------------------------------------------
 * Callback to print received value
 * -------------------------------------------------- */
static void
printReceivedValue(UA_Server *server, void *data) {
    UA_Variant value;
    UA_Variant_init(&value);
    UA_Server_readValue(server, powerLimitTargetNode, &value);

    if(UA_Variant_hasScalarType(&value, &UA_TYPES[UA_TYPES_STRING])) {
        UA_String *str = (UA_String *)value.data;
        if(str->length > 0) {
            printf("\n=======================================================\n");
            printf("[HEAT PUMP] Command received from DSO!\n");
            printf("  Raw command : %.*s\n", (int)str->length, str->data);
            printf("=======================================================\n");
        }
    }
    UA_Variant_clear(&value);
}

/* --------------------------------------------------
 * Main
 * -------------------------------------------------- */
static int
run(UA_String *transportProfile,
    UA_NetworkAddressUrlDataType *networkAddressUrl) {

    UA_Server *server = UA_Server_new();

    addPubSubConnection(server, transportProfile, networkAddressUrl);
    addReaderGroup(server);
    addDataSetReader(server);
    addSubscribedVariables(server, readerIdentifier);

    /* Add a repeated callback to print received value every 1 second */
    UA_UInt64 callbackId;
    UA_Server_addRepeatedCallback(server, printReceivedValue,
                                  NULL, 1000, &callbackId);

    UA_Server_enableAllPubSubComponents(server);

    printf("[HEAT PUMP] C Subscriber started\n");
    printf("[HEAT PUMP] Waiting for DSO power limit command...\n");
    printf("[HEAT PUMP] Listening on: ");
    for(size_t i = 0; i < networkAddressUrl->url.length; i++)
        printf("%c", networkAddressUrl->url.data[i]);
    printf("\n");

    UA_Server_runUntilInterrupt(server);
    UA_Server_delete(server);
    return EXIT_SUCCESS;
}

static void usage(char *progname) {
    printf("usage: %s [uri]\n", progname);
    printf("  default: opc.udp://224.0.0.22:4840/\n");
}

int main(int argc, char **argv) {
    UA_String transportProfile =
        UA_STRING("http://opcfoundation.org/UA-Profile/Transport/pubsub-udp-uadp");
    UA_NetworkAddressUrlDataType networkAddressUrl =
        {UA_STRING_NULL, UA_STRING("opc.udp://224.0.0.22:4840/")};

    if(argc > 1) {
        if(strcmp(argv[1], "-h") == 0) {
            usage(argv[0]);
            return EXIT_SUCCESS;
        } else if(strncmp(argv[1], "opc.udp://", 10) == 0) {
            networkAddressUrl.url = UA_STRING(argv[1]);
        }
    }

    return run(&transportProfile, &networkAddressUrl);
}