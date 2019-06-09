/*
 *
 * Crude directory buster
 * 
 * $ cc cbuster.c -lssl -ltls -lcrypto -o switch_melter
 * 
 * No responsibilty taken for melted NICs
 *
 */

#include <stdio.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <stdbool.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <sys/prctl.h>
#include <signal.h>

void usage(char *argument) {
    fprintf(stderr, "Usage: %s -u <url> -w <wordlist> -x <extensions>\n", argument);
    fprintf(stderr, "       -u          URL\n");
    fprintf(stderr, "       -w          Wordlist\n");
    fprintf(stderr, "       -x          Extensions (optional)\n");
    fprintf(stderr, "       %s -u http://example.com:8000/test/ -w rockyou.txt -x php,txt,html\n", argument);
    exit(EXIT_FAILURE);
}

void validate(char* program_name, char *request, int reqlen, char *test_req, struct addrinfo *res, bool https) {
    ssize_t sentBytes = 0;
    ssize_t recvBytes = 0;
    char buf[1024];
    char *status_code;
    int ret_code, sockfd, sockopt;
    int check_codes[] = 
        {200,201,202,203,204,205,206,207,208,226,300,301,302,303,304,305,306,307,308,401,402,403,407,418,451};
    for (;res != NULL; res->ai_next) {
        sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (sockfd == -1) {
            perror("socket");
            exit(EXIT_FAILURE);
            continue;
        }
        if (connect(sockfd, res->ai_addr, res->ai_addrlen) == -1) {
            perror("connect");
            close(sockfd);
            exit(EXIT_FAILURE);
            continue;
        }
        break;
    }
    if (https == true) {
        const SSL_METHOD *method;
        SSL_CTX *ctx;
        SSL *ssl;
        OpenSSL_add_all_algorithms(); /* Load cryptos */
        SSL_load_error_strings(); /* Bring in and register err msg */
        method = TLSv1_2_client_method(); /* Create new client method */
        ctx = SSL_CTX_new(method); /* Create new context */
        if (ctx == NULL) {
            ERR_print_errors_fp(stderr);
            exit(EXIT_FAILURE);
        }
        ssl = SSL_new(ctx); /* Create new SSL connection state */
        SSL_set_fd(ssl, sockfd); /* attach the socket descriptor */
        if (SSL_connect(ssl) == -1) { /* Perform the connection */
            ERR_print_errors_fp(stderr);
            abort();
        }
        SSL_write(ssl, request, reqlen); /* Encrypt and send */
        SSL_read(ssl, buf, sizeof(buf)); /* Receive and decrypt */
        SSL_free(ssl);
        if (close(sockfd) == -1) {
            perror("shutdown");
            exit(EXIT_FAILURE);
        }
        SSL_CTX_free(ctx); /* Release context */
    } else {
        sentBytes = send(sockfd, request, reqlen, 0);
        if ( !((int)sentBytes == reqlen) )  {
            perror("send");
            exit(EXIT_FAILURE);
        }
        recvBytes = recv(sockfd, buf, sizeof(buf), 0);
        if (close(sockfd) == -1) {
            perror("shutdown");
            exit(EXIT_FAILURE);
        }
    }
    status_code = strchr(buf, ' ');
    status_code++;
    status_code[3] = '\0';
    ret_code = (int)strtol(status_code, NULL, 10);
    for (int i = 0; i < (sizeof(check_codes)/sizeof(check_codes[0])); i++) {
        if (check_codes[i] == ret_code) {
            printf("[%d] %s\n", ret_code, test_req);
            break;
        }
    }
}

void prepare_request(char *program_name, char *tempStr, int post_len, int nread, char *cur_dir_req, char *url_postfix, char *url_addr, struct addrinfo *res, bool https) {
    int request_length;
    char get_request[] = "GET %s HTTP/1.0\r\nHost: %s\r\n\r\n";
    char request[4096];
    const char *comma_delim = ",";
    char *cur_ext = strtok(tempStr, comma_delim);
    while (cur_ext != NULL) {
        int newLen = (post_len-1)+(int)nread+(sizeof(cur_ext));
        char dir_req[newLen];
        char tmp_curDir[nread];
        snprintf(tmp_curDir, nread, "%s", cur_dir_req);
        char *tmpPtr = (char *)(&tmp_curDir+1) - 2;
        tmpPtr = '\0';
        tmpPtr = (char *)(&dir_req+1) - 1;
        if (*cur_ext == ' ') {
            snprintf(dir_req, newLen, "%s%s/", url_postfix, tmp_curDir, cur_ext);
        } else {
            snprintf(dir_req, newLen, "%s%s.%s", url_postfix, tmp_curDir, cur_ext);
        }
        tmpPtr = '\0';
        request_length = snprintf(request, 4096, get_request, dir_req, url_addr);
        pid_t npid = fork();
        if (npid == -1) {
            perror("fork");
            exit(EXIT_FAILURE);
        }
        if (!npid) {
            prctl(PR_SET_PDEATHSIG, SIGKILL);
            validate(program_name, request, request_length, dir_req, res, https);
            exit(EXIT_SUCCESS);
        }
        cur_ext = strtok(NULL, comma_delim);
    }
}

int main(int argc, char **argv) {
    if (argc < 5) { /* Check argument count */
        usage(argv[0]);
    }
    int option = 0;
    int urlf, wlf, xf;
    urlf = wlf = xf = 0;
    while((option = getopt(argc, argv, "u:w:x:")) != -1) {
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
    snprintf(port_string, sizeof(port_string), "%d", port);
    struct addrinfo hints;
    struct addrinfo *res;
    int s;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    s = getaddrinfo(url_addr, port_string, &hints, &res);
    if (s != 0) {
        fprintf(stderr, "getaddrinfo: %s", gai_strerror(s));
        usage(argv[0]);
    }
    char hbuf[NI_MAXHOST];
    socklen_t addrlen;
    if (getnameinfo(res->ai_addr, res->ai_addrlen, hbuf, sizeof(hbuf), NULL, 0, NI_NUMERICHOST) == 0) {
        printf("IPv4: %s\n\n", hbuf);
    }
    char get_request[] = "GET %s HTTP/1.0\r\nHost: %s\r\n\r\n";
    char request[4096];
    char *cur_dir_req = NULL;
    FILE *wordlist;
    size_t len = 0;
    ssize_t nread;
    /* Open file, loop through entries and validate */
    wordlist = fopen(argv[wlf], "r");
    if (wordlist == NULL) {
        perror("fopen");
        usage(argv[0]);
    }
    while ((nread = getline(&cur_dir_req, &len, wordlist) ) != -1) {
        if (xf != 0) {
            char tempStr[sizeof(argv[xf])+1+2];
            char *tmpPtr = (char *)(&argv[xf]+1) - 1;
            tmpPtr = '\0';
            snprintf(tempStr, sizeof(argv[xf])+3, " ,%s", argv[xf]);
            prepare_request(argv[0], tempStr, post_len, nread, cur_dir_req, url_postfix, url_addr, res, https);
        } else {
            char tempStr[] = " ,";
            prepare_request(argv[0], tempStr, post_len, nread, cur_dir_req, url_postfix, url_addr, res, https);
        }
    }
    /* Wait for forks to complete */
    while(1) {
        wait(NULL);
        if (errno == ECHILD) {
            break;
        }
    }

    freeaddrinfo(res);
    free(cur_dir_req);
    fclose(wordlist);

    return 0;
}
