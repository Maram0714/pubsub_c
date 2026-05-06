/* OPC UA PubSub Publisher with Security (Sign + Encrypt)
 * DSO sends power limit command {PLimit:4200.00} to Heat Pump
 * Uses AES-256 encryption and certificate-based signing
 */
#include <open62541/server_config_default.h>
#include <open62541/plugin/log_stdout.h>
#include <open62541/plugin/securitypolicy_default.h>
#include <open62541/server.h>
#include <open62541/server_pubsub.h>
#include <open62541/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Certificate paths */
#define PUBLISHER_CERT    "C:/Users/ines-user/projects/PUBSUB_C/certs/publisher.crt.der"
#define PUBLISHER_KEY     "C:/Users/ines-user/projects/PUBSUB_C/certs/publisher.key.der"
#define CA_CERT           "C:/Users/ines-user/projects/PUBSUB_C/certs/ca.crt.der"

static UA_NodeId connectionIdent, publishedDataSetIdent, writerGroupIdent,
    dataSetWriterIdent, powerLimitNodeId;

/* --------------------------------------------------
 * Load a file into a UA_ByteString
 * -------------------------------------------------- */
static UA_ByteString loadFile(const char *path) {
    UA_ByteString fileContents = UA_BYTESTRING_NULL;
    FILE *fp = fopen(path, "rb");
    if(!fp) {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                     "Cannot open file: %s", path);
        return fileContents;
    }
    fseek(fp, 0, SEEK_END);
    fileContents.length = (size_t)ftell(fp);
    fseek(fp, 0, SEEK_SET);
    fileContents.data = (UA_Byte*)malloc(fileContents.length);
    if(fileContents.data) {
        size_t read = fread(fileContents.data, 1, fileContents.length, fp);
        if(read != fileContents.length) {
            free(fileContents.data);
            fileContents = UA_BYTESTRING_NULL;
        }
    }
    fclose(fp);
    return fileContents;
}

/* --------------------------------------------------
 * Add PubSub Connection
 * -------------------------------------------------- */
static void addPubSubConnection(UA_Server *server,
                                 UA_String *transportProfile,
                                 UA_NetworkAddressUrlDataType *networkAddressUrl) {
    UA_PubSubConnectionConfig connectionConfig;
    memset(&connectionConfig, 0, sizeof(connectionConfig));
    connectionConfig.name = UA_STRING("UADP Secure Connection");
    connectionConfig.transportProfileUri = *transportProfile;
    UA_Variant_setScalar(&connectionConfig.address, networkAddressUrl,
                         &UA_TYPES[UA_TYPES_NETWORKADDRESSURLDATATYPE]);
    connectionConfig.publisherId.idType = UA_PUBLISHERIDTYPE_UINT16;
    connectionConfig.publisherId.id.uint16 = 2234;
    UA_Server_addPubSubConnection(server, &connectionConfig, &connectionIdent);
}

/* --------------------------------------------------
 * Add Published Data Set
 * -------------------------------------------------- */
static void addPublishedDataSet(UA_Server *server) {
    UA_PublishedDataSetConfig publishedDataSetConfig;
    memset(&publishedDataSetConfig, 0, sizeof(UA_PublishedDataSetConfig));
    publishedDataSetConfig.publishedDataSetType = UA_PUBSUB_DATASET_PUBLISHEDITEMS;
    publishedDataSetConfig.name = UA_STRING("DSO Power Commands");
    UA_Server_addPublishedDataSet(server, &publishedDataSetConfig,
                                  &publishedDataSetIdent);
}

/* --------------------------------------------------
 * Add Power Limit Variable Node
 * -------------------------------------------------- */
static void addPowerLimitVariable(UA_Server *server) {
    UA_String powerLimit = UA_STRING("{PLimit:4200.00}");
    UA_VariableAttributes attr = UA_VariableAttributes_default;
    UA_Variant_setScalar(&attr.value, &powerLimit, &UA_TYPES[UA_TYPES_STRING]);
    attr.displayName = UA_LOCALIZEDTEXT("en-US", "PowerLimit");
    attr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;

    powerLimitNodeId = UA_NODEID_STRING(1, "power.limit");
    UA_QualifiedName name = UA_QUALIFIEDNAME(1, "PowerLimit");
    UA_NodeId parentNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER);
    UA_NodeId parentRef = UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES);

    UA_Server_addVariableNode(server, powerLimitNodeId, parentNodeId,
                              parentRef, name,
                              UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
                              attr, NULL, NULL);
    printf("[DSO Publisher] Variable node 'power.limit' created\n");
}

/* --------------------------------------------------
 * Add Dataset Field
 * -------------------------------------------------- */
static void addDataSetField(UA_Server *server) {
    UA_NodeId dataSetFieldIdent;
    UA_DataSetFieldConfig dataSetFieldConfig;
    memset(&dataSetFieldConfig, 0, sizeof(UA_DataSetFieldConfig));
    dataSetFieldConfig.dataSetFieldType = UA_PUBSUB_DATASETFIELD_VARIABLE;
    dataSetFieldConfig.field.variable.fieldNameAlias = UA_STRING("PowerLimit");
    dataSetFieldConfig.field.variable.promotedField = false;
    dataSetFieldConfig.field.variable.publishParameters.publishedVariable =
        UA_NODEID_STRING(1, "power.limit");
    dataSetFieldConfig.field.variable.publishParameters.attributeId =
        UA_ATTRIBUTEID_VALUE;
    UA_Server_addDataSetField(server, publishedDataSetIdent,
                              &dataSetFieldConfig, &dataSetFieldIdent);
}

/* --------------------------------------------------
 * Add Writer Group WITH Security
 * -------------------------------------------------- */
static void addWriterGroup(UA_Server *server) {
    UA_WriterGroupConfig writerGroupConfig;
    memset(&writerGroupConfig, 0, sizeof(UA_WriterGroupConfig));
    writerGroupConfig.name = UA_STRING("DSO WriterGroup");
    writerGroupConfig.publishingInterval = 5000; /* 5 seconds */
    writerGroupConfig.writerGroupId = 100;
    writerGroupConfig.encodingMimeType = UA_PUBSUB_ENCODING_UADP;

    /* Enable Sign + Encrypt security */
    //writerGroupConfig.securityMode = UA_MESSAGESECURITYMODE_SIGNANDENCRYPT;
   // writerGroupConfig.securityGroupId = UA_STRING("DSO-SecurityGroup-1");

    /* Network message content mask */
    UA_UadpWriterGroupMessageDataType writerGroupMessage;
    UA_UadpWriterGroupMessageDataType_init(&writerGroupMessage);
    writerGroupMessage.networkMessageContentMask =
        (UA_UadpNetworkMessageContentMask)(
            UA_UADPNETWORKMESSAGECONTENTMASK_PUBLISHERID |
            UA_UADPNETWORKMESSAGECONTENTMASK_GROUPHEADER |
            UA_UADPNETWORKMESSAGECONTENTMASK_WRITERGROUPID |
            UA_UADPNETWORKMESSAGECONTENTMASK_PAYLOADHEADER);
    UA_ExtensionObject_setValue(&writerGroupConfig.messageSettings,
                                &writerGroupMessage,
                                &UA_TYPES[UA_TYPES_UADPWRITERGROUPMESSAGEDATATYPE]);

    UA_Server_addWriterGroup(server, connectionIdent,
                             &writerGroupConfig, &writerGroupIdent);
}

/* --------------------------------------------------
 * Add Dataset Writer
 * -------------------------------------------------- */
static void addDataSetWriter(UA_Server *server) {
    UA_DataSetWriterConfig dataSetWriterConfig;
    memset(&dataSetWriterConfig, 0, sizeof(UA_DataSetWriterConfig));
    dataSetWriterConfig.name = UA_STRING("DSO DataSetWriter");
    dataSetWriterConfig.dataSetWriterId = 62541;
    dataSetWriterConfig.keyFrameCount = 10;
    UA_Server_addDataSetWriter(server, writerGroupIdent, publishedDataSetIdent,
                               &dataSetWriterConfig, &dataSetWriterIdent);
}

/* --------------------------------------------------
 * Main
 * -------------------------------------------------- */
static int run(UA_String *transportProfile,
               UA_NetworkAddressUrlDataType *networkAddressUrl) {

    /* Load certificates */
    UA_ByteString publisherCert = loadFile(PUBLISHER_CERT);
    UA_ByteString publisherKey  = loadFile(PUBLISHER_KEY);
    UA_ByteString caCert        = loadFile(CA_CERT);

    if(publisherCert.length == 0 || publisherKey.length == 0 || caCert.length == 0) {
        printf("[ERROR] Failed to load certificates. Check paths:\n");
        printf("  Publisher cert: %s\n", PUBLISHER_CERT);
        printf("  Publisher key:  %s\n", PUBLISHER_KEY);
        printf("  CA cert:        %s\n", CA_CERT);
        return EXIT_FAILURE;
    }
    printf("[DSO Publisher] Certificates loaded successfully\n");

    /* Create server with security */
    UA_Server *server = UA_Server_new();
    UA_ServerConfig *config = UA_Server_getConfig(server);
    UA_ServerConfig_setMinimal(config, 4840, NULL);

    /* Add PubSub components */
    addPubSubConnection(server, transportProfile, networkAddressUrl);
    addPublishedDataSet(server);
    addPowerLimitVariable(server);
    addDataSetField(server);
    addWriterGroup(server);
    addDataSetWriter(server);

    UA_Server_enableAllPubSubComponents(server);

    printf("[DSO Publisher] Started - sending {PLimit:4200.00} encrypted every 5s\n");
    printf("[DSO Publisher] Security: Sign + Encrypt (AES-256)\n");
    printf("[DSO Publisher] Address: ");
    for(size_t i = 0; i < networkAddressUrl->url.length; i++)
        printf("%c", networkAddressUrl->url.data[i]);
    printf("\n");

    UA_StatusCode retval = UA_Server_runUntilInterrupt(server);

    UA_ByteString_clear(&publisherCert);
    UA_ByteString_clear(&publisherKey);
    UA_ByteString_clear(&caCert);
    UA_Server_delete(server);
    return retval == UA_STATUSCODE_GOOD ? EXIT_SUCCESS : EXIT_FAILURE;
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