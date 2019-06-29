/*
 *
 * C directory buster
 * 
 * $ cc cbuster.c -ldill -o cbuster
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <libdill.h>

void usage(char *argument) {
    fprintf(stderr, "cbuster v1.0 - written by postrequest\n", argument);
    fprintf(stderr, "\nOptions:\n", argument);
    fprintf(stderr, "  -u          URL\n");
    fprintf(stderr, "  -w          Wordlist\n");
    fprintf(stderr, "  -x          Extensions (optional)\n");
    fprintf(stderr, "  -d          Add header to request (optional)\n");
    fprintf(stderr, "  -t          Threads (default: 10)\n");
    fprintf(stderr, "\nUsage:\n");
    fprintf(stderr, "  %s -u <url> -w <wordlist> -x <extensions>\n", argument);
    fprintf(stderr, "  %s -u https://example.com:8000/test/ -w rockyou.txt -x php,txt,html -d \"Authorization: YWRtaW46YWRtaW4=\" -t 60\n", argument);
    exit(EXIT_FAILURE);
}

void generate_random_string(char *randomstring, int size) {
    srand(now());
    int i; 
    char base[62]="abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890";
    for (i = 0; i < (size-1); i++) {
        randomstring[i] = base[rand() % (sizeof(base) - 1)];
    }
    randomstring[(size-1)] = '\0';
}

coroutine void prepare_request(char *dir_req, char *url_addr, char *headers, struct ipaddr *res, bool https, bool wildcard) {
    int request_length;
    char *get_request;
    char request[4096];
    if (headers != "") {
        get_request = "GET %s HTTP/1.0\r\nHost: %s\r\n%s\r\n\r\n";
        request_length = snprintf(request, 4096, get_request, dir_req, url_addr, headers);
    } else {
        get_request = "GET %s HTTP/1.0\r\nHost: %s\r\n\r\n";
        request_length = snprintf(request, 4096, get_request, dir_req, url_addr);
    }
    /* Send the request */
    char buf[1024];
    char *status_code;
    int ret_code, sockfd;
    int check_codes[] = 
        {200,201,202,203,204,205,206,207,208,226,300,301,302,303,304,305,306,307,308,401,402,403,407,418,451};
    sockfd = tcp_connect(res, -1);
    if (sockfd == -1) {
        fprintf(stderr, "[X] Could not connect socket, maybe too many requests\n");
        exit(EXIT_FAILURE);
    }
    if (https == true) {
        sockfd = tls_attach_client(sockfd, -1);
        if (sockfd == -1) {
            fprintf(stderr, "[X] Error creating TLS protocol on underlying socket\n");
            exit(EXIT_FAILURE);
        }
    }
    bsend(sockfd, request, request_length, -1);
    brecv(sockfd, buf, sizeof(buf), -1);
    if (https == true) {
        sockfd = tls_detach(sockfd, -1);
    }
    tcp_close(sockfd, -1);
    status_code = strchr(buf, ' ');
    status_code++;
    status_code[3] = '\0';
    ret_code = (int)strtol(status_code, NULL, 10);
    for (int i = 0; i < (sizeof(check_codes)/sizeof(check_codes[0])); i++) {
        if (check_codes[i] == ret_code) {
            if (wildcard) {
                printf("[X] Wilcard URL match\n\n");
                printf("[%d] %s\n\n", ret_code, dir_req);
                printf("[*] Exiting\n\n");
                exit(EXIT_SUCCESS);
            }
            printf("[%d] %s\n", ret_code, dir_req);
            break;
        }
    }
    /* Request sent */
}

int main(int argc, char **argv) {
    if (argc < 5) { /* Check argument count */
        usage(argv[0]);
    }
    int option = 0;
    int urlf, wlf, xf, df, tf;
    urlf = wlf = xf = df = tf = 0;
    while((option = getopt(argc, argv, "u:w:x:d:t:")) != -1) {
        switch (option) { /* Loop through command line flags */
            case 'u':
                urlf = optind-1;
                break;
            case 'w':
                wlf = optind-1;
                break;
            case 'x':
                xf = optind-1;
                break;
            case 'd':
                df = optind-1;
                break;
            case 't':
                tf = optind-1;
                break;
            default:
                usage(argv[0]);
        }
    }
    if(urlf == 0 || wlf == 0) { /* No url or wordlist != good */
        usage(argv[0]);
    }
    unsigned int port = 80;
    char port_string[6];
    char *url_token;
    char *url_addr;
    char *urlPtr;
    char *headers;
    const char *slash_delim = "/";
    bool https = false;
    url_token = strtok(argv[urlf], slash_delim);
    if (strcmp("http:", url_token) == 0) { /* HTTP Service */
        port = 80;
        url_addr = argv[urlf]+strlen("http://");
    } else if (strcmp("https:", url_token) == 0) { /* HTTPS Service */
        port = 443;
        url_addr = argv[urlf]+strlen("https://");
        https = true;
    }
    urlPtr = strchr(url_addr, *slash_delim); /* Check for directory after domain */
    if (urlPtr == NULL) {
        urlPtr = (char *)slash_delim;
    }
    int post_len = strlen(urlPtr)+1;
    char url_postfix[post_len+1];
    strncpy(url_postfix, urlPtr, post_len);
    char *tmpPtr = &url_postfix[post_len-2];
    if (*tmpPtr != '/') { /* Ensure dir has a / */
        *(++tmpPtr) = '/';
        *(++tmpPtr) = '\0';
    }
    if (urlPtr != slash_delim) {
        urlPtr[0] = '\0';
    }
    urlPtr = strchr(url_addr, ':'); /* Check if port specified after domain */
    if (urlPtr != NULL) {
        port = (unsigned int)strtoul(urlPtr+1, NULL, 10);
        urlPtr[0] = '\0';
    }
    printf("Target: %s//%s:%d\n", url_token, url_addr, port);
    printf("Directory: %s\n", url_postfix);
    if (df != 0) { /* Check if data is appended to request */
        headers = argv[df];
        printf("Data: %s\n", headers);
    } else {
        headers = "";
    }
    int threads;
    if (tf != 0) { /* Check for user defined threads */
        threads = (int)strtol(argv[tf], NULL, 10);
    } else {
        threads = 10;
    }
    printf("Threads: %d\n", threads);
    snprintf(port_string, sizeof(port_string), "%d", port);
    struct ipaddr res;
    int s;
    s = ipaddr_remote(&res, url_addr, port, 0, -1);
    if (s != 0) {
        fprintf(stderr, "[X] Error resolving remote address\n");
        usage(argv[0]);
    }
    char hbuf[17];
    ipaddr_str(&res, hbuf);
    printf("IPv4: %s\n\n", hbuf);
    char *cur_dir_req_nl = NULL;
    FILE *wordlist;
    size_t len = 0;
    ssize_t nread;
    /* Open file, loop through entries and validate */
    wordlist = fopen(argv[wlf], "r");
    if (wordlist == NULL) {
        perror("fopen");
        usage(argv[0]);
    }
    int bees = bundle();
    int val = 0;
    /* Generate  and test for wildcard */
    int randomStringSize = 225;
    char randomstring[randomStringSize];
    generate_random_string((char *)&randomstring, randomStringSize);
    char wildcardHTTP[sizeof(url_postfix)+randomStringSize+1];
    snprintf(&wildcardHTTP[0], (sizeof(url_postfix)+randomStringSize+1), "%s%s", url_postfix, randomstring);
    prepare_request(wildcardHTTP, url_addr, headers, &res, https, true);
    /* Loop through wordlist */
    while ((nread = getline(&cur_dir_req_nl, &len, wordlist) ) != -1) {
        /* current way manage threads, change to FIFO instead of wait */
        if (val == threads) {
            bundle_wait(bees, -1);
            val = 0;
        }
        /* remove '\n' from word */
        char cur_dir_req[nread];
        snprintf(cur_dir_req, nread, "%s", cur_dir_req_nl);
        cur_dir_req[nread] = '\0';
        if (xf != 0) {
            char tempStr[sizeof(argv[xf])+1+2];
            char *tmpPtr = (char *)(&argv[xf]+1) - 1;
            tmpPtr = '\0';
            snprintf(tempStr, sizeof(argv[xf])+3, " ,%s", argv[xf]);
            const char *comma_delim = ",";
            char *cur_ext = strtok(tempStr, comma_delim);
            while (cur_ext != NULL) {
                /* current way manage threads, change to FIFO instead of wait */
                if (val == threads) {
                    bundle_wait(bees, -1);
                    val = 0;
                }
                int newLen = (post_len-1)+(int)nread+(sizeof(cur_ext));
                char dir_req[newLen];
                if (*cur_ext == ' ') {
                    snprintf(dir_req, newLen, "%s%s", url_postfix, cur_dir_req, cur_ext);
                } else {
                    snprintf(dir_req, newLen, "%s%s.%s", url_postfix, cur_dir_req, cur_ext);
                }
                bundle_go(bees, prepare_request(dir_req, url_addr, headers, &res, https, false));
                cur_ext = strtok(NULL, comma_delim);
                val++;
            }
        } else {
            char dir_req[sizeof(url_postfix) + nread];
            snprintf(dir_req, (sizeof(url_postfix) + nread), "%s%s", url_postfix, cur_dir_req);
            bundle_go(bees, prepare_request(dir_req, url_addr, headers, &res, https, false));
            val++;
        }
        /* Check for directory */
        char dir_req[sizeof(url_postfix) + nread+1];
        snprintf(dir_req, (sizeof(url_postfix) + nread), "%s%s/", url_postfix, cur_dir_req);
        bundle_go(bees, prepare_request(dir_req, url_addr, headers, &res, https, false));
        val++;
    }
    bundle_wait(bees, -1);
    hclose(bees);
    free(cur_dir_req_nl);
    fclose(wordlist);

    return 0;
}
