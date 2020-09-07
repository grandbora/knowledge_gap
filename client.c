#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#define CHUNK_SIZE 512


void error(const char *msg)
{
    perror(msg);
    exit(0);
}

void check_arguments(int argc, char *argv[])
{
    if (argc < 3) {
       fprintf(stderr,"usage %s hostname port\n", argv[0]);
       exit(0);
    }
}

void log_status(char *buffer, int next_line_start)
{
    printf("Status```%.*s```\n", next_line_start - 2, buffer);
}

void log_header(char *buffer, int next_line_start)
{
    printf("Header```%.*s```\n", next_line_start - 2, buffer);
}

void log_body(char *buffer)
{
    printf("BODY```%s```\n", buffer);
}

int connect_to_host(char hostname_arg[], char port_arg[])
{
    int sockfd, portno;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    portno = atoi(port_arg);
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");
    server = gethostbyname(hostname_arg);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,
         (char *)&serv_addr.sin_addr.s_addr,
         server->h_length);
    serv_addr.sin_port = htons(portno);

    if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0)
        error("ERROR connecting");

    return sockfd;
}

void make_get_request(int sockfd)
{
    char get_req[] = "GET / HTTP/1.1\r\n\r\n";
    int byte_count = write(sockfd, get_req, strlen(get_req));
    if (byte_count < 0) error("ERROR writing to socket");
}

void load_buffer(int sockfd, char *buffer, int chunk_size)
{
    // printf("load_buffer before buffer: ```%s```\n", buffer);
    // printf("load_buffer before buffer len: ```%lu```\n", strlen(buffer));
    int buffer_len = strlen(buffer);
    for(int i = 0; i < buffer_len; i++)
    {
        buffer++;
    }
    // printf("load_buffer mid buffer: ```%s```\n", buffer);

    int received_byte_count = read(sockfd, buffer, chunk_size);
    if (received_byte_count < 0)
        error("ERROR: Can not read the header chunk");
    // printf("load_buffer after buffer: ```%s```\n", buffer);
}

int find_next_line(char *buffer)
{
    char *result = strstr(buffer, "\r\n");
    if(!result) return -1;
    int position = result - buffer + 2;
    // printf("find_next_line buffer: ```%s```, position: ```%d```\n", buffer, position);
    return position;
}

void release_line(char *buffer, int next_line_start)
{
    // printf("relese before buffer: ```%s```\n", buffer);

    char *next_line = buffer + next_line_start;
    // printf("relese before next_line: ```%s```\n", next_line);

    int rest_headers_len = strlen(buffer) - next_line_start;
    // printf("relese before rest_headers_len: ```%d```\n", rest_headers_len);

    memmove(buffer, next_line, rest_headers_len);

    // separate old string leftovers
    buffer[rest_headers_len] = '\0';

    // printf("relese after buffer: ```%s```\n", buffer);
}

void process_status(char *buffer)
{
    int next_line_start = find_next_line(buffer);
    if(next_line_start == -1) {
        fprintf(stderr, "Http response status line could not be found.\n Response: ```%s``` \n", buffer);
        exit(0);
    }
    log_status(buffer, next_line_start);
    release_line(buffer, next_line_start);
}

void copy_if_content_length(char *buffer, int next_line_start, char *content_length)
{
    if(memcmp(buffer, "Content-Length: ", 16) != 0) return;
    for(int i=0; i<16; i++) {
        buffer++;
    }

    memcpy(content_length, buffer, next_line_start - 16 - 2);
    // printf("in copy copied content_length: ```%s```\n", content_length);
}

bool is_message_body_start(char *buffer)
{
    return buffer[0] == '\r' && buffer[1] == '\n';
}

void parse_headers(int sockfd, char *buffer, char *content_length)
{
    bool is_status = true;
    bool reached_message_body = false;
    while(!reached_message_body){
        load_buffer(sockfd, buffer, CHUNK_SIZE);
        // printf("buffer: ```%s```\n", buffer);

        if(is_status) {
            process_status(buffer);
            is_status = false;
        }

        int next_line_start = find_next_line(buffer);
        while(next_line_start != -1 && !reached_message_body) {
            log_header(buffer, next_line_start);
            copy_if_content_length(buffer, next_line_start, content_length);
            release_line(buffer, next_line_start);
            reached_message_body = is_message_body_start(buffer);
            if(!reached_message_body) {
                next_line_start = find_next_line(buffer);
            }
        }
    }
}

void fail_if_not_content_length(int sockfd, char *content_length)
{
    if(content_length[0] == '\0') {
        printf("Non content-length specified responses are not supported.\n");
        close(sockfd);
        exit(0);
    }
}

void remove_CRLF_from_body_start(char *buffer)
{
    char *body_start = buffer + 2;
    int rest_body_len = strlen(buffer) - 2;
    memmove(buffer, body_start, rest_body_len);
    buffer[rest_body_len] = '\0';
}

void load_body(int sockfd, char *buffer, int parsed_content_length)
{
    remove_CRLF_from_body_start(buffer);

    int missing_body_bytes = parsed_content_length - strlen(buffer);
    while(missing_body_bytes > 0) {
        load_buffer(sockfd, buffer, parsed_content_length - strlen(buffer));
        missing_body_bytes = parsed_content_length - strlen(buffer);
    }

    log_body(buffer);
}

int main(int argc, char *argv[])
{
    check_arguments(argc, argv);

    char buffer[4096]="";
    char content_length[256]="";

    int sockfd = connect_to_host(argv[1], argv[2]);
    make_get_request(sockfd);
    parse_headers(sockfd, buffer, content_length);
    fail_if_not_content_length(sockfd, content_length);

    int parsed_content_length = atoi(content_length);
    load_body(sockfd, buffer, parsed_content_length);

    close(sockfd);
    return 0;
}
