/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);

int main(int argc, char **argv) {
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  // 명령줄 인자가 올바른지 확인
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  // 포트 번호로 수신 대기 소켓 열기
  listenfd = Open_listenfd(argv[1]);

  // 무한 루프 : 클라이언트의 연결 요청을 반복적으로 수락하고 처리
  while (1) {
    clientlen = sizeof(clientaddr); // 클라이언트 주소 구조체의 크기
    connfd = Accept(listenfd, (SA *)&clientaddr, 
                    &clientlen);  // 연결 요청 수락
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE,
                0); // 클라이언트의 이름과 포트 얻기
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);   // HTTP 트랜잭션 처리
    Close(connfd);  // 연결 종료
  }
}

// 클라이언트의 HTTP 요청을 처리하고 응답을 생성해 보내는 함수
void doit(int fd)
{
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio;
    /**
     * rio_t : 
     * - 버퍼링된 I/O 구조체, 데이터를 버퍼에 읽어와 저장해둠. 
     * - 여러번 작은 데이터 읽는 경우에도 버퍼에서 필요한 만큼 가져오므로 시스템 콜을 최소화할 수 있음.
     */

  Rio_readinitb(&rio, fd);
  Rio_readlineb(&rio, buf, MAXLINE);
  printf("Request headers:\n");
  printf("%s", buf);

  // 요청 라인을 파싱하여 메소드, URI, 버전을 분리
  sscanf(buf, "%s %s %s", method, uri, version);

  // GET 메소드가 아닌 경우 501 오류 반환
  // 501 에러 : 서버가 요청된 메서드를 지원하지 않거나 구현되지 않음. 
  if (strcasecmp(method, "GET")){
    clienterror(fd, method, "501", "Not implemented", "Tiny dose not implement this method");
    return;
  }

  // 요청 헤더를 읽고 무시
  read_requesthdrs(&rio);

  // URI를 파싱하여 파일 이름과 CGI 인자 분리
  is_static = parse_uri(uri, filename, cgiargs);

  // 파일이 존재하는지 확인
  // 404 에러 : 서버가 요청한 리소스를 찾을 수 없음. stat함수가 요청된 파일을 찾지 못할 때 404오류 발생
  if (stat(filename, &sbuf) < 0) {
        clienterror(fd, filename, "404", "Not found", "Tiny couldn’t find this file");
        return;
  }

  // 403 에러 : 서버가 요청된 리소스에 대해 엑세스를 거부함.
  // 클라이언트가 읽기, 실행 권한이 부족한 경우.
  if (is_static){ // 정적 콘텐츠 제공
    // 읽기 가능한 일반 파일인지 확인
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)){
      clienterror(fd, filename, "403", "Forbidden", "Tiny coulen't read the file");
      return;
    }
    serve_static(fd, filename, sbuf.st_size); // 정적 파일 전송 함수 호출
  }
  else { // 동적 콘텐츠 제공
    // 실행 가능한 파일인지 확인
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn’t run the CGI program");
      return;
    }
    serve_dynamic(fd, filename, cgiargs); // 동적 콘텐츠 전송 함수 호출
  }
}

// Tiny 웹서버에서 HTTP 오류 응답을 생성하는 함수
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg){
  // buf : 헤더 버퍼, body : 바디 버퍼
  char buf[MAXLINE], body[MAXBUF]; 

  // HTML 형식의 응답 본문을 생성
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);  // 오류 코드와 짧은 메시지
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause); // 상세 설명과 원인
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body); // 서버 이름

  // HTTP 응답 헤더를 생성하고 전송
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf)); // 응답 줄 전송
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf)); // 콘텐츠 유형 전송
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf)); // 콘텐츠 길이 전송

  // HTML 본문 전송
  Rio_writen(fd, body, strlen(body));
  // Rio_
}

// Tiny 웹 서버가 클라이언트 요청의 헤더 부분을 읽고 무시하는 함수
void read_requesthdrs(rio_t *rp){
  char buf[MAXLINE];

  // 요청 헤더 한 줄 읽음
  Rio_readlineb(rp, buf, MAXLINE);

  // 빈 줄("\r\n")이 나올 때까지 계속 읽음
  while(strcmp(buf, "\r\n")){
    Rio_readlineb(rp, buf, MAXLINE); // 다음 줄 읽기
    printf("%s", buf); // 헤더 내용을 출력 (디버깅용)
  }
  return;
}

// HTTP 요청에서 받은 URI를 파싱해서 정적 or 동적 콘텐츠를 구분하고, 파일 이름과 CGI 인자를 분리함.
int parse_uri(char *uri, char *filename, char *cgiargs){
  char *ptr;

  // 요청이 정적 콘텐츠인지 확인(URI에 "cgi-bin"이 포함되지 않으면 정적 콘텐츠)
  if (!strstr(uri, "cgi-bin")){
    // CGI 인자 초기화
    strcpy(cgiargs, "");

    // 현재 디렉터리를 기준으로 파일 경로 설정
    strcpy(filename, ".");
    strcat(filename, uri);

    // URI가 '/'로 끝나면 기본 파일명을 추가
    if(uri[strlen(uri)-1] == '/')
      strcat(filename, "home.html");
    return 1;
  }
  else  // 동적 콘텐츠
  {
    // URI에 '?'문자 있으면, CGI 인자로 분리
    ptr = index(uri, '?');
    if (ptr){
      strcpy(cgiargs, ptr + 1); // '?' 뒤의 문자열을 CGI 인자로 저장
      *ptr = '\0'; // '?'를 '\0'으로 변경하여 파일명 부분만 남김
    }
    else 
    {
      strcpy(cgiargs, ""); // CGI 인자가 없으면 빈 문자열로 초기화
    }

    // 현재 디렉터리를 기준으로 파일 경로 설정
    strcpy(filename, ".");
    strcat(filename, uri);
    return 0;
  }
}

// 정적 파일 전송 함수 
void serve_static(int fd, char *filename, int filesize){
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF];

  // 클라이언트에게 전송할 응답 헤더를 설정
  get_filetype(filename, filetype); // 파일 확장자를 기반으로 MIME 타입 설정
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
  Rio_writen(fd, buf, strlen(buf)); // 설정된 헤더를 클라이언트에게 전송
  printf("Response headers:\n");
  printf("%s", buf);

  // 클라이언트에게 전송할 응답 본문을 설정
  srcfd = Open(filename, O_RDONLY, 0); // 파일을 읽기 전용으로 열기
  // srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0); // 파일을 가상메모리에 매핑

  // Q11.9 Mmap대신 malloc 사용 -> 빈칸에 사용해야하므로 빈 공간에 메모리 읽어야함.
  srcp = (char *)malloc(filesize);
  Rio_readn(srcfd, srcp, filesize);

  Close(srcfd);                   // 파일 디스크립터 닫기 (메모리에 매핑되어 있으므로 필요 없음)
  Rio_writen(fd, srcp, filesize); // 파일 내용을 클라이언트에 전송
  // Munmap(srcp, filesize); // 매핑된 메모리 해제
  free(srcp); // 동적할당 해제
}

// 동적 콘텐츠를 클라이언트에 제공하는 함수
void serve_dynamic(int fd, char *filename, char *cgiargs)
{
  char buf[MAXLINE], *emptylist[] = {NULL};

  // HTTP 응답의 첫 부분 전송 (응답 라인과 서버 정보)
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));

  if (Fork() == 0) { // 자식 프로세스
    /* 실서버에서는 모든 CGI 변수들을 설정 */
    setenv("QUERY_STRING", cgiargs, 1); // CGI 인자 설정
    Dup2(fd, STDOUT_FILENO);            // 표준 출력을 클라이언트로 리다이렉트
    Execve(filename, emptylist, environ); // CGI 프로그램 실행
  }
  Wait(NULL); /* 부모 프로세스는 자식이 끝날 때까지 대기*/
}

// 파일 형식 추출하기
void get_filetype(char *filename, char *filetype) {
    if (strstr(filename, ".html"))
        strcpy(filetype, "text/html"); // HTML 파일
    else if (strstr(filename, ".gif"))
        strcpy(filetype, "image/gif"); // GIF 이미지 파일
    else if (strstr(filename, ".png"))
        strcpy(filetype, "image/png"); // PNG 이미지 파일
    else if (strstr(filename, ".jpg"))
        strcpy(filetype, "image/jpeg"); // JPEG 이미지 파일
    else if (strstr(filename, ".mp4"))
        strcpy(filetype, "video/mp4"); // mp4 비디오 파일
    else
        strcpy(filetype, "text/plain"); // 기본 타입: 일반 텍스트 파일
}
