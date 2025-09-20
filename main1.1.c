#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <windows.h>
#include <direct.h>
#include <sys/stat.h>
#pragma comment(lib,"ws2_32.lib")

#define MAXBUF 8192
#define WWW_DIR "www"
#define PHP_CGI "php\\php-cgi.exe"

int load_port(void) {
    FILE *f = fopen("host.json","r");
    if(!f) return 8080;
    char buf[256]; int port=8080;
    fread(buf,1,sizeof(buf)-1,f);
    fclose(f);
    if (sscanf(buf,"{\"port\":%d}",&port)==1) return port;
    return 8080;
}

void log_request(const char *ip,const char *req) {
    FILE *f=fopen("access.log","a");
    if(!f) return;
    SYSTEMTIME st; GetLocalTime(&st);
    fprintf(f,"%04d-%02d-%02d %02d:%02d:%02d %s %s\n",
            st.wYear,st.wMonth,st.wDay,st.wHour,st.wMinute,st.wSecond,
            ip,req);
    fclose(f);
}

int file_exists(const char *path) {
    struct _stat st;
    return _stat(path,&st)==0;
}

int is_php(const char *path) {
    const char *ext = strrchr(path,'.');
    return ext && _stricmp(ext,".php")==0;
}

char* run_php(const char *script) {
    SECURITY_ATTRIBUTES sa = {sizeof(SECURITY_ATTRIBUTES), NULL, TRUE};
    HANDLE out_read, out_write;
    if(!CreatePipe(&out_read,&out_write,&sa,0)) return NULL;
    SetHandleInformation(out_read,HANDLE_FLAG_INHERIT,0);

    char cmd[512];
    snprintf(cmd,sizeof(cmd),"%s -q \"%s\"", PHP_CGI, script);

    PROCESS_INFORMATION pi; STARTUPINFO si;
    ZeroMemory(&pi,sizeof(pi)); ZeroMemory(&si,sizeof(si));
    si.cb=sizeof(si);
    si.dwFlags|=STARTF_USESTDHANDLES;
    si.hStdOutput=out_write;
    si.hStdError=out_write;
    si.hStdInput=GetStdHandle(STD_INPUT_HANDLE);

    if(!CreateProcess(NULL,cmd,NULL,NULL,TRUE,CREATE_NO_WINDOW,NULL,NULL,&si,&pi)){
        CloseHandle(out_write); CloseHandle(out_read);
        return NULL;
    }
    CloseHandle(out_write);

    char *buf = malloc(MAXBUF*16);
    if(!buf){ CloseHandle(out_read); return NULL; }
    size_t len=0; DWORD n;
    while(ReadFile(out_read,buf+len,MAXBUF,&n,NULL) && n>0) len+=n;
    CloseHandle(out_read);
    WaitForSingleObject(pi.hProcess,INFINITE);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    buf[len]=0;
    return buf;
}

const char* get_mime(const char *path){
    const char *ext = strrchr(path,'.');
    if(!ext) return "text/plain";
    if(_stricmp(ext,".html")==0||_stricmp(ext,".htm")==0) return "text/html";
    if(_stricmp(ext,".css")==0) return "text/css";
    if(_stricmp(ext,".js")==0) return "application/javascript";
    if(_stricmp(ext,".png")==0) return "image/png";
    if(_stricmp(ext,".jpg")==0||_stricmp(ext,".jpeg")==0) return "image/jpeg";
    if(_stricmp(ext,".gif")==0) return "image/gif";
    if(_stricmp(ext,".ico")==0) return "image/x-icon";
    return "application/octet-stream";
}

int main() {
    WSADATA wsa;
    if(WSAStartup(MAKEWORD(2,2),&wsa)!=0) { printf("WSAStartup failed\n"); return 1; }

    int port = load_port();
    SOCKET s = socket(AF_INET,SOCK_STREAM,0);
    if(s==INVALID_SOCKET) { printf("Socket failed\n"); return 1; }

    struct sockaddr_in server;
    server.sin_family=AF_INET;
    server.sin_addr.s_addr=INADDR_ANY;
    server.sin_port=htons(port);

    if(bind(s,(struct sockaddr*)&server,sizeof(server))<0){ printf("Bind failed\n"); return 1; }
    if(listen(s,5)<0){ printf("Listen failed\n"); return 1; }

    printf("Server is running on http://localhost:%d\n",port);

    while(1){
        struct sockaddr_in client; int clen=sizeof(client);
        SOCKET cs=accept(s,(struct sockaddr*)&client,&clen);
        if(cs==INVALID_SOCKET) { printf("Accept failed\n"); continue; }

        char ip[32]; strcpy(ip,inet_ntoa(client.sin_addr));
        char req[MAXBUF]; int r=recv(cs,req,sizeof(req)-1,0);
        if(r<=0){ closesocket(cs); continue; }
        req[r]=0;

        char method[16], path[256], ver[16];
        sscanf(req,"%15s %255s %15s", method, path, ver);
        log_request(ip, req);

        if(strstr(path,"..")) { send(cs,"HTTP/1.1 403 Forbidden\r\n\r\nForbidden",31,0); closesocket(cs); continue; }

        char full[512];
        if(strcmp(path,"/")==0){
            snprintf(full,sizeof(full),"%s\\index.php",WWW_DIR);
            if(!file_exists(full)) snprintf(full,sizeof(full),"%s\\index.html",WWW_DIR);
            if(!file_exists(full)) snprintf(full,sizeof(full),"%s\\index.htm",WWW_DIR);
        } else {
            snprintf(full,sizeof(full),"%s%s",WWW_DIR,path);
        }

        if(!file_exists(full)) { send(cs,"HTTP/1.1 404 Not Found\r\n\r\nNot Found",36,0); closesocket(cs); continue; }

        if(is_php(full)){
            char *out = run_php(full);
            if(!out){ send(cs,"HTTP/1.1 500 Internal Server Error\r\n\r\nCGI Error",47,0); closesocket(cs); continue; }
            // Pošli základní hlavičku pokud PHP nevrací
            send(cs,"HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n",44,0);
            send(cs,out,strlen(out),0);
            free(out);
        } else {
            const char *mime = get_mime(full);
            char header[256];
            snprintf(header,sizeof(header),"HTTP/1.1 200 OK\r\nContent-Type: %s\r\n\r\n",mime);
            send(cs,header,strlen(header),0);

            FILE *f=fopen(full,"rb");
            if(!f){ send(cs,"HTTP/1.1 500 Internal Server Error\r\n\r\nCannot open file",47,0); closesocket(cs); continue; }
            char buf[4096]; size_t n;
            while((n=fread(buf,1,sizeof(buf),f))>0) send(cs,buf,(int)n,0);
            fclose(f);
        }
        closesocket(cs);
    }

    closesocket(s);
    WSACleanup();
    return 0;
}
