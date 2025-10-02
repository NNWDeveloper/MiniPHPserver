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

typedef struct {
    SOCKET client;
    struct sockaddr_in addr;
} client_info;

// ----------- Konfigurace serveru -----------
int load_port(void) {
    FILE *f = fopen("host.json","r");
    if(!f) return 8080;
    char buf[256]; int port=8080;
    fread(buf,1,sizeof(buf)-1,f);
    fclose(f);
    sscanf(buf,"{\"port\":%d}", &port);
    return port;
}

// ----------- Logging -----------------------
void log_request(const char *ip, const char *method, const char *path, int code) {
    FILE *f = fopen("access.log","a");
    if(!f) return;
    SYSTEMTIME st; GetLocalTime(&st);
    fprintf(f,"%04d-%02d-%02d %02d:%02d:%02d %s \"%s %s\" %d\n",
            st.wYear,st.wMonth,st.wDay,st.wHour,st.wMinute,st.wSecond,
            ip, method, path, code);
    fclose(f);
}

// ----------- Souborové utility ------------
int file_exists(const char *path) {
    struct _stat st;
    return _stat(path,&st)==0;
}

int is_php(const char *path) {
    const char *ext = strrchr(path,'.');
    return ext && _stricmp(ext,".php")==0;
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

// ----------- Hezké chyby HTML ------------
void send_html_error(SOCKET cs, int code, const char *title, const char *message) {
    char html[1024];
    snprintf(html, sizeof(html),
        "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>%d %s</title>"
        "<style>body{font-family:Arial,sans-serif;background:#f2f2f2;color:#333;padding:50px;text-align:center;}"
        "h1{font-size:50px;}p{font-size:20px;}</style></head><body>"
        "<h1>%d %s</h1><p>%s</p></body></html>", code, title, code, title, message);

    char header[256];
    snprintf(header,sizeof(header),
             "HTTP/1.1 %d %s\r\nContent-Type: text/html; charset=UTF-8\r\nContent-Length: %llu\r\nConnection: close\r\n\r\n",
             code, title, (unsigned long long)strlen(html));
    send(cs, header, strlen(header), 0);
    send(cs, html, strlen(html), 0);
}

// ----------- Spuštìní PHP ------------------
char* run_php(const char *script, const char *method, const char *query, const char *post_data, size_t post_len, const char *ip) {
    SECURITY_ATTRIBUTES sa = {sizeof(SECURITY_ATTRIBUTES), NULL, TRUE};
    HANDLE out_read, out_write, in_read, in_write;

    if(!CreatePipe(&out_read,&out_write,&sa,0)) return NULL;
    SetHandleInformation(out_read,HANDLE_FLAG_INHERIT,0);
    if(!CreatePipe(&in_read,&in_write,&sa,0)) return NULL;
    SetHandleInformation(in_write,HANDLE_FLAG_INHERIT,0);

    char cmd[512];
    snprintf(cmd,sizeof(cmd),"%s -q \"%s\"", PHP_CGI, script);

    PROCESS_INFORMATION pi; STARTUPINFO si;
    ZeroMemory(&pi,sizeof(pi)); ZeroMemory(&si,sizeof(si));
    si.cb=sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = out_write;
    si.hStdError  = out_write;
    si.hStdInput  = in_read;

    // Nastavení environment promìnných
    _putenv_s("REQUEST_METHOD", method);
    _putenv_s("QUERY_STRING", query ? query : "");
    char clen[32]; snprintf(clen,sizeof(clen),"%zu", post_len);
    _putenv_s("CONTENT_LENGTH", clen);
    _putenv_s("SCRIPT_FILENAME", script);
    _putenv_s("SCRIPT_NAME", script);
    _putenv_s("SERVER_PROTOCOL", "HTTP/1.1");
    _putenv_s("REMOTE_ADDR", ip);
    _putenv_s("REDIRECT_STATUS","1"); // nutné pro CGI

    if(!CreateProcess(NULL, cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        CloseHandle(out_write); CloseHandle(out_read);
        CloseHandle(in_write); CloseHandle(in_read);
        return NULL;
    }
    CloseHandle(out_write);
    CloseHandle(in_read);

    if(post_data && post_len>0){
        DWORD written;
        WriteFile(in_write, post_data, (DWORD)post_len, &written, NULL);
    }
    CloseHandle(in_write);

    char *buf = malloc(MAXBUF*16);
    if(!buf){ CloseHandle(out_read); CloseHandle(pi.hProcess); CloseHandle(pi.hThread); return NULL; }

    size_t len=0; DWORD n;
    while(ReadFile(out_read, buf+len, MAXBUF, &n, NULL) && n>0) len+=n;
    CloseHandle(out_read);
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    buf[len]=0;
    return buf;
}

// ----------- Zpracování jednoho klienta -------
DWORD WINAPI handle_client(LPVOID arg) {
    client_info *ci = (client_info*)arg;
    SOCKET cs = ci->client;
    char ip[32]; strcpy(ip, inet_ntoa(ci->addr.sin_addr));
    free(ci);

    char req[MAXBUF];
    int r = recv(cs, req, sizeof(req)-1, 0);
    if(r <= 0){ closesocket(cs); return 0; }
    req[r] = 0;

    char method[16], path[256], ver[16];
    sscanf(req, "%15s %255s %15s", method, path, ver);

    char *query = strchr(path, '?');
    if(query){ *query = 0; query++; }

    if(strstr(path,"..") || path[0] != '/') {
        log_request(ip, method, path, 403);
        send_html_error(cs, 403, "Forbidden", "Access to this resource is denied.");
        closesocket(cs); return 0;
    }

    char full[512];
    if(strcmp(path,"/")==0){
        snprintf(full,sizeof(full),"%s\\index.php",WWW_DIR);
        if(!file_exists(full)) snprintf(full,sizeof(full),"%s\\index.html",WWW_DIR);
        if(!file_exists(full)) snprintf(full,sizeof(full),"%s\\index.htm",WWW_DIR);
    } else snprintf(full,sizeof(full),"%s%s",WWW_DIR,path);

    if(!file_exists(full)){
        log_request(ip, method, path, 404);
        send_html_error(cs, 404, "Not Found", "The requested file or directory was not found on this server.");
        closesocket(cs); return 0;
    }

    char *post_data = NULL;
    size_t post_len = 0;
    if(_stricmp(method,"POST")==0){
        char *cl_hdr = strstr(req,"Content-Length:");
        if(cl_hdr){
            int content_length=0;
            sscanf(cl_hdr,"Content-Length: %d",&content_length);
            if(content_length>0){
                post_data = malloc(content_length+1);
                if(post_data){
                    char *body = strstr(req,"\r\n\r\n");
                    if(body){
                        body += 4;
                        int body_len = r - (body - req);
                        if(body_len>0){
                            memcpy(post_data, body, body_len);
                            post_len = body_len;
                        }
                    }
                    post_data[post_len]=0;
                }
            }
        }
    }

    if(is_php(full)){
        char *out = run_php(full, method, query, post_data, post_len, ip);
        if(post_data) free(post_data);
        if(!out){ log_request(ip, method, path, 500); send_html_error(cs,500,"Server Error","PHP execution failed."); closesocket(cs); return 0; }

        // Vyjmeme PHP hlavièky
        char *body = strstr(out, "\r\n\r\n");
        if(body) body += 4;

        char header[256];
        snprintf(header,sizeof(header),"HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=UTF-8\r\nConnection: close\r\n\r\n");
        send(cs,header,strlen(header),0);
        send(cs, body ? body : out, strlen(body ? body : out), 0);
        free(out);
        log_request(ip, method, path, 200);
    } else {
        if(post_data) free(post_data);
        const char *mime = get_mime(full);
        FILE *f = fopen(full,"rb");
        if(!f){ log_request(ip, method, path, 500); send_html_error(cs,500,"Server Error","Cannot open file."); closesocket(cs); return 0; }

        fseek(f,0,SEEK_END);
        long filesize = ftell(f); fseek(f,0,SEEK_SET);

        char header[256];
        if(strncmp(mime,"text/",5)==0){
            snprintf(header,sizeof(header),
                     "HTTP/1.1 200 OK\r\nContent-Type: %s; charset=UTF-8\r\nContent-Length: %llu\r\nConnection: close\r\n\r\n",
                     mime, (unsigned long long)filesize);
        } else {
            snprintf(header,sizeof(header),
                     "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %llu\r\nConnection: close\r\n\r\n",
                     mime, (unsigned long long)filesize);
        }
        send(cs,header,strlen(header),0);

        char buf[4096]; size_t n;
        while((n=fread(buf,1,sizeof(buf),f))>0) send(cs,buf,(int)n,0);
        fclose(f);
        log_request(ip, method, path, 200);
    }

    closesocket(cs);
    return 0;
}

// ----------- Hlavní funkce -----------------
int main() {
    WSADATA wsa;
    if(WSAStartup(MAKEWORD(2,2),&wsa)!=0){ printf("WSAStartup failed\n"); return 1; }

    int port = load_port();
    SOCKET s = socket(AF_INET,SOCK_STREAM,0);
    if(s==INVALID_SOCKET){ printf("Socket failed\n"); return 1; }

    struct sockaddr_in server;
    server.sin_family=AF_INET;
    server.sin_addr.s_addr=INADDR_ANY;
    server.sin_port=htons(port);

    if(bind(s,(struct sockaddr*)&server,sizeof(server))<0){ printf("Bind failed\n"); return 1; }
    if(listen(s,5)<0){ printf("Listen failed\n"); return 1; }

    printf("Server is running on http://localhost:%d\n",port);

    while(1){
        struct sockaddr_in client; int clen = sizeof(client);
        SOCKET cs = accept(s,(struct sockaddr*)&client,&clen);
        if(cs==INVALID_SOCKET){ printf("Accept failed\n"); continue; }

        client_info *ci = malloc(sizeof(client_info));
        ci->client = cs;
        ci->addr = client;

        HANDLE th = CreateThread(NULL,0,handle_client,ci,0,NULL);
        if(th) CloseHandle(th);
    }

    closesocket(s);
    WSACleanup();
    return 0;
}
