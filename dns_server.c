#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdbool.h>
#include <cjson/cJSON.h>    // for parsing config file
// #include <glib.h>           // for set
#include <gmodule.h>        // for single list

#define DNS_PORT 53
#define BUFFER_SIZE 512

const char* const config_file = "server.config";
GSList* blacklist = NULL;
/* default server settings */
const char* upper_dns_ip = "8.8.8.8";
const char* refused_answer = "Not allowed";

/* struct for saving data for sending dns request to the upper server */
typedef struct _dnsdata {
    int sfd;
    const char* data;
    ssize_t data_len;
    struct sockaddr_in* client_addr;
    struct sockaddr_in* dns_addr;
} dnsdata_t;

void errExit(const char* msg)
{
    perror(msg);
    exit(1);
}

char* readConfigFile(const char* const file_name)
{
    /* Get the file size */
    struct stat statBuf;
    if (stat(file_name, &statBuf) == -1) {
        errExit("statBuf");
    }

    int fd = open(file_name, O_RDONLY);
    if (fd == -1) {
        perror("open config file");
        return NULL;
    }
    
    char* fileDataBuf;
    ssize_t fileSize = statBuf.st_size;
    fileDataBuf = (char*)malloc(fileSize + 1);   // must be freed in initServerConfig func
    if (fileDataBuf == NULL) {
        errExit("malloc");
    }

    if (read(fd, fileDataBuf, fileSize) == -1) {
        errExit("read config file");
    }
    fileDataBuf[fileSize] = '\0';

    return fileDataBuf;
}

void initServerConfig(const char* const config_file)
{
    const cJSON* blocked_site = NULL;
    const cJSON* blacklist_json = NULL;
    const cJSON* upper_server = NULL;
    const cJSON* answer_json = NULL;

    printf("...Server configuration...\n");

    char* config_file_data = readConfigFile(config_file);
    if (config_file_data == NULL) {
        printf(">> Server will use the default settings\n");
        return;
    }

    cJSON* config_json = cJSON_Parse(config_file_data);
    free(config_file_data);
    if (config_json == NULL) {
        const char* error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
            fprintf(stderr, ">> Can't parse JSON. Error before: %s\n", error_ptr);
            
        }
        cJSON_Delete(config_json);
        printf(">> Server will use the default settings\n");
        return;
    }

    upper_server = cJSON_GetObjectItemCaseSensitive(config_json, "upper_server_ip");
    if (cJSON_IsString(upper_server) && (upper_server->valuestring != NULL)) {
        upper_dns_ip = upper_server->valuestring;
        printf(">> Upper server is set to '%s'\n", upper_dns_ip);
    }

    answer_json = cJSON_GetObjectItemCaseSensitive(config_json, "answer");
    if (cJSON_IsString(answer_json) && (answer_json->valuestring != NULL)) {
        refused_answer = answer_json->valuestring;
        printf(">> Refused answer is set to '%s'\n", refused_answer);
    }

    blacklist_json = cJSON_GetObjectItemCaseSensitive(config_json, "blacklist");
    int number = 0;
    cJSON_ArrayForEach(blocked_site, blacklist_json) {
        if (cJSON_IsString(blocked_site) && (blocked_site->valuestring != NULL)) {
            char* str = blocked_site->valuestring;

            /* replacing '.' symbols for for correct processing of dns requests*/
            for (int i = 0; i < strlen(str); i++) {
                if (str[i] == '.') {
                    str[i] = 0x03;
                }
            }
            blacklist = g_slist_prepend(blacklist, str);
            ++number;
        }
    }
    printf(">> Added %d domain(s) to the blacklist\n\n", number);
}

void proxyDNSRequest(dnsdata_t* dnsinfo) {       
    socklen_t client_addrlen = sizeof(struct sockaddr_in);
    socklen_t dns_addrlen = sizeof(struct sockaddr_in);

    char answer[BUFFER_SIZE];
    memset(answer, 0, BUFFER_SIZE);

    sendto(dnsinfo->sfd, dnsinfo->data, dnsinfo->data_len, 0, (struct sockaddr *)dnsinfo->dns_addr, dns_addrlen);
    ssize_t answer_len = recvfrom(dnsinfo->sfd, answer, BUFFER_SIZE, 0, (struct sockaddr *)dnsinfo->dns_addr, &dns_addrlen);

    // Skip DNS header, question sect and some answer sect
    char* domain = answer + 12;
    domain += strlen(domain) + 5;
    domain += strlen(domain) + 10;
    printf("IP answer from external dns server: %d.%d.%d.%d\n", (unsigned char)domain[0], (unsigned char)domain[1], (unsigned char)domain[2], (unsigned char)domain[3]);

    sendto(dnsinfo->sfd, answer, answer_len, 0, (struct sockaddr *)dnsinfo->client_addr, client_addrlen);
}

bool isBlocked(char* request)
{
    /* iterate through blacklist to check the requested domain is blocked */
    for  (GSList* blacklist_iter = blacklist; blacklist_iter != NULL; blacklist_iter = blacklist_iter->next) {
        if (strstr(request, (char*)blacklist_iter->data) != NULL)
            return true;
    }
    return false;
}

int main(int argc, char* argv[])
{
    struct sockaddr_in server_addr, client_addr, default_dns_addr;
    socklen_t client_addrlen;
    ssize_t request_len;
    char client_addr_str[INET_ADDRSTRLEN];
    char request[BUFFER_SIZE];

    initServerConfig(config_file);

    /* configure upper server */
    memset(&default_dns_addr, 0, sizeof(struct sockaddr_in));
    default_dns_addr.sin_family = AF_INET;
    default_dns_addr.sin_port = htons(DNS_PORT);
    if (inet_pton(AF_INET, upper_dns_ip, &default_dns_addr.sin_addr) < 0)
        errExit(("inet_pton failed for address '%s'", upper_dns_ip));

    dnsdata_t dns_info;
    dns_info.dns_addr = &default_dns_addr;

    int sfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sfd == -1)
        errExit("server_socket");

    dns_info.sfd = sfd;
    
    memset(&server_addr, 0, sizeof(struct sockaddr_in));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(DNS_PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sfd, (struct sockaddr*)&server_addr, sizeof(struct sockaddr_in)) == -1)
        errExit("bind");

    printf("DNS proxy server is running...\n");
    
    for (;;) {
        memset(request, 0, BUFFER_SIZE);
        memset(&client_addr, 0, sizeof(struct sockaddr_in));
        client_addrlen = sizeof(client_addr);

        request_len = recvfrom(sfd, request, BUFFER_SIZE, 0, (struct sockaddr*)&client_addr, &client_addrlen);
        if (request_len == -1) {
            perror("recvfrom");
            continue;
        }
        
        if (inet_ntop(AF_INET, &client_addr.sin_addr, client_addr_str, INET_ADDRSTRLEN) == NULL)
            perror("Can't convert client address to a presentation form.\n");
        
        char* domain = request + 12; // Skipping DNS header
        
        if (isBlocked(domain)) {
            printf("Request %s is blocked\n", domain);

            /* we could send answer from config file: */
            // sendto(sfd, refused_answer, strlen(refused_answer), 0, (struct sockaddr *)&client_addr, client_addrlen);

            /* but I think that is better to set DNS response codes to REFUSED: */
            request[2] |= 0x80;                         // Set answer in header
            request[3] = (request[3] & 0XF0) | 0x05;    // Set refused code
            sendto(sfd, request, request_len, 0, (struct sockaddr *)&client_addr, client_addrlen);
        } else {
            printf("Forwarding request for %s...\n", domain);
            dns_info.data = request;
            dns_info.data_len = request_len;
            dns_info.client_addr = &client_addr;
            proxyDNSRequest(&dns_info);
        }
        puts("");
    }

    return 0;
}