#include "csapp.h"

int main(int argc, char **argv)
{
    int clientfd;
    char *host, *port, buf[MAXLINE];
    rio_t rio;
    /**
     * rio_t : 
     * - 버퍼링된 I/O 구조체, 데이터를 버퍼에 읽어와 저장해둠. 
     * - 여러번 작은 데이터 읽는 경우에도 버퍼에서 필요한 만큼 가져오므로 시스템 콜을 최소화할 수 있음.
     */

    // 입력 인자 확인 : 프로그램 이름 외에 호스트와 포트가 필요하다.
    if (argc != 3){
        fprintf(stderr, "usage: %s <host> <port>\n, argv[0]");
        exit(0);
    }

    host = argv[1]; // 서버 호스트 이름 또는 IP 주소
    port = argv[2]; // 서버 포트 번호

    clientfd = Open_clientfd(host, port); // 서버에 연결하기 위한 소켓 생성 및 연결
    Rio_readinitb(&rio, clientfd); // 연결된 소켓을 버퍼링된 읽기 구조체로 초기화

    // 표준 입력으로부터 텍스트 라인을 읽어서 서버에 전송하고 응답 받음
    while (Fgets(buf, MAXLINE, stdin) != NULL){ // 사용자 입력을 buf에 저장
        Rio_writen(clientfd, buf, strlen(buf)); // buf의 내용을 서버로 전송, 버퍼 모든 내용을 한번에 보냄.
        // Rio_readinitb(&rio, MAXLINE); // 서버로부터 응답을 읽어 buf에 저장,
        Rio_readlineb(&rio, buf, MAXLINE); // 서버로부터 응답을 읽어 buf에 저장,
        Fputs(buf, stdout); // 서버에 응답을 표준 출력(stdout)으로 출력
    }
    Close(clientfd); // 모든 작업이 끝나면 서버와의 연결을 닫음
    exit(0);
}