/* echoserver.c : 특정 포트에서 연결 요청을 수락하고, 연결된 클라이언트와 데이터를 주고받는 에코 서버*/

#include "csapp.h"

void echo(int connfd); // 클라이언트와의 에코 처리를 담당하는 함수 선언

void echo(int connfd)
{
    size_t n;
    char buf[MAXLINE];
    rio_t rio;

    Rio_readinitb(&rio, connfd);
    while ((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0)
        printf("server received %d bytes\n", (int)n);
        Rio_writen(connfd, buf, n);
}

int main(int argc, char **argv) 
{
    int listenfd, connfd; // 수신 대기 소켓과 연결 소켓 디스크립터
    socklen_t clientlen;
    struct sockaddr_storage clientaddr; // 클라이언트 주소 정보 저장 구조체
    char client_hostname[MAXLINE], client_port[MAXLINE];

    if (argc != 2){ // 인자가 제대로 입력되지 않으면 사용법 출력
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }

    listenfd = Open_listenfd(argv[1]); // 주어진 포트로 수신 대기 소켓 생성
    while (1){

        clientlen = sizeof(struct sockaddr_storage); //클라이언트 주소 구조체 크기 설정
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); // 클라이언트 연결 수락
        Getnameinfo((SA *)&clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0); // 연결된 클라이언트 정보 가져오기
        printf("Connected to (%s, %s)\n", client_hostname, client_port); // 연결된 클라이언트 정보 출력
        echo(connfd); // 클라이언트와의 데이터 교환 처리
        Close(connfd); // 연결 소켓 닫기
    }
}

