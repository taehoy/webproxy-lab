#include <stdio.h>
#include "csapp.h"
/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *conn_hdr = "Connection: close\r\n";
static const char *prox_hdr = "Proxy-Connection: close\r\n";
static const char *host_hdr_format = "Host: %s\r\n";
static const char *requestlint_hdr_format = "GET %s HTTP/1.0\r\n";
static const char *endof_hdr = "\r\n";

static const char *connection_key = "Connection";
static const char *user_agent_key = "User-Agent";
static const char *proxy_connection_key = "Proxy-Connection";
static const char *host_key = "Host";

// 클라이언트에서 서버로의 통신 함수
void doit(int connfd);
// 클라이언트 요청에서 URI 파싱하는 함수
void parse_uri(char *uri,char *hostname, char *path, int *port);
void build_http_header(char *http_header, char *hostname, char *path, int port, rio_t *client_rio);
// 서버와의 연결 설정 함수
int connect_endServer(char *hostname, int port);

int main(int argc, char **argv)
{
    int listenfd, connfd;
    socklen_t  clientlen;
    char hostname[MAXLINE], port[MAXLINE];

    // 클라이언트 주소를 저장할 구조체
    struct sockaddr_storage clientaddr;

    // 포트 번호 입력되지 않았을 경우 오류 메시지 출력
    if(argc != 2){
        fprintf(stderr,"usage :%s <port> \n",argv[0]);
        exit(1);
    }

    // 수신 대기 소켓을 열기 위한 파일 디스크립터
    listenfd = Open_listenfd(argv[1]);

    // 프록시 서버가 무한 루프 내에서 클라이언트 요청 수신
    while(1) {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); // 클라이언트 연결

        // 클라이언트주소를 hostname과 port 문자열 반환
        Getnameinfo((SA*)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
        printf("Accepted connection from (%s %s).\n", hostname, port);

        doit(connfd);       // 클라이언트 요청 처리
        Close(connfd);      // 클라이언트와이 연결 종료
    }
    return 0;
}


/* 클라이언트 요청을 처리하는 함수 */
void doit(int connfd)
{
    int end_serverfd;           // 최종 서버와의 연결을 위한 파일 디스크립터
    int port;                   // URI에서 추출한 포트 번호 저장

    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char endserver_http_header[MAXLINE];
    char hostname[MAXLINE], path[MAXLINE];
    
    rio_t rio, server_rio; // 클라이언트와 서버의 버퍼 관리

    Rio_readinitb(&rio, connfd);                // 클라이언트 fd로 rio 버퍼 초기화
    Rio_readlineb(&rio, buf, MAXLINE);          // 클라이언트 요청의 첫 번째 줄 읽기
    sscanf(buf,"%s %s %s", method, uri, version);

    // GET 메서드가 아닌 요청은 오류 처리 후 함수 종료
    if(strcasecmp(method, "GET")){
        printf("Proxy does not implement the method");
        return;
    }

    parse_uri(uri, hostname, path, &port);     // URI를 호스트명, 경로, 포트로 파싱
    // 최종 서버에 보낼 HTTP 헤더 생성    
    build_http_header(endserver_http_header, hostname, path, port, &rio);

    /* 최종 서버와의 연결 설정 */
    end_serverfd = connect_endServer(hostname, port);
    if(end_serverfd < 0) {
        printf("connection failed\n");
        return;
    }

    // 최종 서버와의 연결을 위해 server_rio 초기화
    Rio_readinitb(&server_rio, end_serverfd);  
    
    // 최종 서버로 HTTP 헤더 전송
    Rio_writen(end_serverfd, endserver_http_header, strlen(endserver_http_header));


    // 최종 서버로부터 받은 응답을 클라이언트에 전송
    size_t n;
    while((n=Rio_readlineb(&server_rio,buf,MAXLINE))!=0)
    {
        printf("proxy received %d bytes,then send\n",n);
        Rio_writen(connfd,buf,n);       // 클라이언트로 응답 전송
    }
    Close(end_serverfd);                // 최종 서버와의 연결 종료
}

/* 최종 서버에 보낼 HTTP 헤더 구성*/
void build_http_header(char *http_header, char *hostname, char *path, int port, rio_t *client_rio)
{
    // 클라이언트 요청 데이터를 읽어서 임시 저장하는 버퍼들 선언
    char buf[MAXLINE], request_hdr[MAXLINE], other_hdr[MAXLINE], host_hdr[MAXLINE];
   
    // request_hdr에 GET 요청 라인을 생성해서 저장. path는 요청 경로가 들어있음
    sprintf(request_hdr, requestlint_hdr_format, path);
    
    /* 클라이언트의 헤더 요청을 읽어오고 필요한 정보 수정해서 사용 */
    while(Rio_readlineb(client_rio, buf, MAXLINE) > 0)
    {
        // 읽어들인 값(buf)가 /r/n이면 break
        if(strcmp(buf, endof_hdr) == 0)
        {
            break;
        }

        // "Host:" 헤더가 있으면 host_hdr에 복사해서 저장
        if(!strncasecmp(buf, host_key, strlen(host_key)))/*Host:*/
        {
            // 호스트 정보가 포함된 buf 내용을 host_hdr에 복사
            strcpy(host_hdr, buf);
            continue;
        }

        // "connection", "Proxy-Connection", "User-Agent" 이외의 header 만들어주기
        if(!strncasecmp(buf, connection_key, strlen(connection_key))
                &&!strncasecmp(buf, proxy_connection_key, strlen(proxy_connection_key))
                &&!strncasecmp(buf, user_agent_key, strlen(user_agent_key)))
        {
            strcat(other_hdr, buf);     // other_hdr에 해당 헤더 추가
        }
    }

    // 클라이언트 요청에 "Host:" 헤더가 없으면, 호스트 이름으로 새 "Host:" 헤더를 만들어 추가
    if(strlen(host_hdr) == 0)
    {
        sprintf(host_hdr, host_hdr_format, hostname);
    }

    // 완전체 만들어주기
    // 순서: 요청 라인 -> 호스트 헤더 -> 기본 헤더 (연결 관련) -> 유저 에이전트 헤더 -> 기타 헤더 -> 헤더 종료
    sprintf(http_header, "%s%s%s%s%s%s%s", request_hdr, host_hdr, conn_hdr, prox_hdr, user_agent_hdr, other_hdr, endof_hdr);
    return ;
}

/*Connect to the end server*/
// inline int connect_endServer(char *hostname, int port, char *http_header){
inline int connect_endServer(char *hostname, int port){
    char portStr[100];
    // portstr에 port 넣어주기
    sprintf(portStr, "%d", port);
    // 해당 hostname과 portStr로 end_server에게 가는 요청만들어주기
    return Open_clientfd(hostname, portStr);
}

/* URI를 파싱해서 호스트명, 경로, 포스 번호 추출 함수 */
void parse_uri(char *uri,char *hostname,char *path,int *port)
{
    *port = 80;                     // 기본 포트를 80으로 설정 (포트 정보가 없으면 기본값 사용)

    // uri 문자열에서 "//"의 위치를 찾음. 
    char* pos = strstr(uri, "//");  

    // "//"가 존재하면 그 다음 위치(pos+2)를 시작 위치로, 존재하지 않으면 uri 처음부터 시작.
    pos = pos != NULL? pos+2:uri;

    // ":"의 위치를 찾음. 포트번호 있는 경우
    char* pos2 = strstr(pos, ":");

    // ":"가 존재할 경우 
    if(pos2 != NULL)
    {
        *pos2 = '\0';                       // ":"-> '\0'으로 설정
        sscanf(pos, "%s", hostname);        // 호스트명을 hostname에 저장
        sscanf(pos2+1, "%d%s", port, path); // ":"다음부터 포트, 경로 파싱해서 각각 port와 path에 저장
    }
    else                                    // ":"가 존재하지 않는 경우
    {
        pos2 = strstr(pos,"/");             // "/"의 위치를 찾음. 

        if(pos2!=NULL)                      // 경로가 존재하는 경우
        {
            *pos2 = '\0';                   // 호스트명만 추출하기 위해 "/"를 '\0'으로 설정
            sscanf(pos,"%s",hostname);      // 호스트명을 hostname에 저장
            *pos2 = '/';                    // "/"를 원래대로 복구
            sscanf(pos2,"%s",path);         // "/"로 부터 시작하는 경로를 path에 저장
        }
        else                                // "/"도 존재하지않는 경우(포트x, 경로x)
        {
            sscanf(pos,"%s",hostname);      // 호스트명만 hostname에 저장
        }
    }
    return;
}