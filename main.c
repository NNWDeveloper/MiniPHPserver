#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <windows.h>
#include <direct.h>  // pro _chdir()


#pragma comment(lib, "ws2_32.lib")

#define PORT 8080
#define BUFFER_SIZE 4096
#define PHP_CGI_PATH ".\\php\\php-cgi.exe"  // cesta k PHP
#define WWW_DIR ".\\www"                    // složka s webem

void handle_client(SOCKET client) {
    char buffer[BUFFER_SIZE];
    int received = recv(client, buffer, sizeof(buffer)-1, 0);
    if (received <= 0) { closesocket(client); return; }

    buffer[received] = '\0';
    char method[8], path[256];
    sscanf(buffer, "%s %s", method, path);

    // výchozí index
    if (strcmp(path, "/") == 0) strcpy(path, "/index.php");

    char fullpath[512];
    snprintf(fullpath, sizeof(fullpath), "%s%s", WWW_DIR, path);

    // detekce PHP souboru
    char *ext = strrchr(fullpath, '.');
    if (ext && strcmp(ext, ".php") == 0) {
        char command[1024];
        snprintf(command, sizeof(command), "%s -f \"%s\" 2>&1", PHP_CGI_PATH, fullpath);

        FILE* fp = _popen(command, "r");
        if (fp) {
            char output[BUFFER_SIZE];
            send(client, "HTTP/1.1 200 OK\r\n\r\n", 19, 0);
            while (fgets(output, sizeof(output), fp)) {
                send(client, output, strlen(output), 0);
            }
            _pclose(fp);
        } else {
            send(client, "HTTP/1.1 500 Internal Server Error\r\n\r\nPHP Error\n", 45, 0);
        }
    } else {
        // statický soubor (HTML, CSS, JS)
        FILE* fp = fopen(fullpath, "rb");
        if (fp) {
            send(client, "HTTP/1.1 200 OK\r\n\r\n", 19, 0);
            while (!feof(fp)) {
                int n = fread(buffer, 1, sizeof(buffer), fp);
                send(client, buffer, n, 0);
            }
            fclose(fp);
        } else {
            send(client, "HTTP/1.1 404 Not Found\r\n\r\nFile not found\n", 42, 0);
        }
    }

    closesocket(client);
}

int main() {
    // nastavení pracovního adresáře
    _chdir(".");  // aby fungovala relativní cesta k PHP

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) { printf("WSAStartup failed\n"); return 1; }

    SOCKET server = socket(AF_INET, SOCK_STREAM, 0);
    if (server == INVALID_SOCKET) { printf("Socket error\n"); return 1; }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        printf("Bind error: %d\n", WSAGetLastError());
        return 1;
    }

    if (listen(server, 10) == SOCKET_ERROR) {
        printf("Listen error\n");
        return 1;
    }

    printf("Server is running on port %d...\n", PORT);

    while (1) {
        SOCKET client = accept(server, NULL, NULL);
        if (client != INVALID_SOCKET) {
            handle_client(client);
        }
    }

    closesocket(server);
    WSACleanup();
    return 0;
}
