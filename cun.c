#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <iso646.h>
#include <lua5.1/lauxlib.h>
#include <lua5.1/lua.h>
#include <netinet/in.h>
#include <readline/readline.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <threads.h>
#include <unistd.h>

#include "arena.c"
#include "request.c"
#include "result.c"

extern void luaL_openlibs(lua_State* L);  // my headers don't have this for some reason

extern int fileno(FILE*);

static void shutdownWrapper(void* data) {
    shutdown(*(int*)data, SHUT_RDWR);
}

static void lua_closeWrapper(void* data) {
    lua_close(*(lua_State**)data);
}

static char const* requestMethodToString(RequestMethod method) {
    switch (method) {
        case GET_REQUEST:
            return "get";
        case POST_REQUEST:
            return "post";
        case HEAD_REQUEST:
            return "head";
        case PUT_REQUEST:
            return "put";
        case PATCH_REQUEST:
            return "patch";
        case DELETE_REQUEST:
            return "delete";
    }
}

static char const* responsePhrase(int code) {
    switch (code) {
        case 200:
            return "OK";
        case 404:
            return "Not Found";
        default:
            return "CODE";
    }
}

typedef struct RequestHandlerInput RequestHandlerInput;
struct RequestHandlerInput {
    int socket;
    char ip[16];
};

static char const* getMimeType(String filename) {
    if (filename.length == 0) {
        return "application/octet-stream";
    }
    size_t extension_start;
    for (extension_start = filename.length - 1; extension_start > 0; extension_start--) {
        if (filename.data[extension_start] == '/') {
            return "application/octet-stream";
        }
        if (filename.data[extension_start] == '.') {
            break;
        }
    }

    String extension = LSTRING(filename.data + extension_start, filename.length - extension_start);
    if (stringCompare(extension, STRING_LITERAL(".htm")) == 0 or
        stringCompare(extension, STRING_LITERAL(".html")) == 0) {
        return "text/html";
    } else if (stringCompare(extension, STRING_LITERAL(".css")) == 0) {
        return "text/css";
    } else if (stringCompare(extension, STRING_LITERAL(".js")) == 0) {
        return "application/javascript";
    } else if (stringCompare(extension, STRING_LITERAL(".png")) == 0) {
        return "image/png";
    } else if (stringCompare(extension, STRING_LITERAL(".jpg")) == 0 or
               stringCompare(extension, STRING_LITERAL(".jpeg")) == 0) {
        return "image/jpeg";
    } else if (stringCompare(extension, STRING_LITERAL(".svg")) == 0) {
        return "image/svg+xml";
    } else if (stringCompare(extension, STRING_LITERAL(".json")) == 0) {
        return "application/json";
    } else if (stringCompare(extension, STRING_LITERAL(".txt")) == 0) {
        return "text/plain";
    } else if (stringCompare(extension, STRING_LITERAL(".lua")) == 0) {
        return "text/x-lua";
    } else if (stringCompare(extension, STRING_LITERAL(".luac")) == 0) {
        return "application/x-lua-bytecode";
    } else if (stringCompare(extension, STRING_LITERAL(".aac")) == 0) {
        return "audio/aac";
    } else if (stringCompare(extension, STRING_LITERAL(".abw")) == 0) {
        return "application/x-abiword";
    } else if (stringCompare(extension, STRING_LITERAL(".apng")) == 0) {
        return "image/apng";
    } else if (stringCompare(extension, STRING_LITERAL(".arc")) == 0) {
        return "application/x-freearc";
    } else if (stringCompare(extension, STRING_LITERAL(".avif")) == 0) {
        return "image/avif";
    } else if (stringCompare(extension, STRING_LITERAL(".avi")) == 0) {
        return "video/x-msvideo";
    } else if (stringCompare(extension, STRING_LITERAL(".azw")) == 0) {
        return "application/vnd.amazon.ebook";
    } else if (stringCompare(extension, STRING_LITERAL(".bin")) == 0) {
        return "application/octet-stream";
    } else if (stringCompare(extension, STRING_LITERAL(".bmp")) == 0) {
        return "image/bmp";
    } else if (stringCompare(extension, STRING_LITERAL(".bz")) == 0) {
        return "application/x-bzip";
    } else if (stringCompare(extension, STRING_LITERAL(".bz2")) == 0) {
        return "application/x-bzip2";
    } else if (stringCompare(extension, STRING_LITERAL(".cda")) == 0) {
        return "application/x-cdf";
    } else if (stringCompare(extension, STRING_LITERAL(".csh")) == 0) {
        return "application/x-csh";
    } else if (stringCompare(extension, STRING_LITERAL(".csv")) == 0) {
        return "text/csv";
    } else if (stringCompare(extension, STRING_LITERAL(".doc")) == 0) {
        return "application/msword";
    } else if (stringCompare(extension, STRING_LITERAL(".docx")) == 0) {
        return "application/vnd.openxmlformats-officedocument.wordprocessingml.document";
    } else if (stringCompare(extension, STRING_LITERAL(".eot")) == 0) {
        return "application/vnd.ms-fontobject";
    } else if (stringCompare(extension, STRING_LITERAL(".epub")) == 0) {
        return "application/epub+zip";
    } else if (stringCompare(extension, STRING_LITERAL(".gz")) == 0) {
        return "application/gzip";
    } else if (stringCompare(extension, STRING_LITERAL(".gif")) == 0) {
        return "image/gif";
    } else if (stringCompare(extension, STRING_LITERAL(".ico")) == 0) {
        return "image/vnd.microsoft.icon";
    } else if (stringCompare(extension, STRING_LITERAL(".ics")) == 0) {
        return "text/calendar";
    } else if (stringCompare(extension, STRING_LITERAL(".jar")) == 0) {
        return "application/java-archive";
    } else if (stringCompare(extension, STRING_LITERAL(".jsonld")) == 0) {
        return "application/ld+json";
    } else if (stringCompare(extension, STRING_LITERAL(".md")) == 0) {
        return "text/markdown";
    } else if (stringCompare(extension, STRING_LITERAL(".mid")) == 0 or
               stringCompare(extension, STRING_LITERAL(".midi")) == 0) {
        return "audio/midi";
    } else if (stringCompare(extension, STRING_LITERAL(".mjs")) == 0) {
        return "text/javascript";
    } else if (stringCompare(extension, STRING_LITERAL(".mp3")) == 0) {
        return "audio/mpeg";
    } else if (stringCompare(extension, STRING_LITERAL(".mp4")) == 0) {
        return "video/mp4";
    } else if (stringCompare(extension, STRING_LITERAL(".mpeg")) == 0) {
        return "video/mpeg";
    } else if (stringCompare(extension, STRING_LITERAL(".mpkg")) == 0) {
        return "application/vnd.apple.installer+xml";
    } else if (stringCompare(extension, STRING_LITERAL(".odp")) == 0) {
        return "application/vnd.oasis.opendocument.presentation";
    } else if (stringCompare(extension, STRING_LITERAL(".ods")) == 0) {
        return "application/vnd.oasis.opendocument.spreadsheet";
    } else if (stringCompare(extension, STRING_LITERAL(".odt")) == 0) {
        return "application/vnd.oasis.opendocument.text";
    } else if (stringCompare(extension, STRING_LITERAL(".oga")) == 0) {
        return "audio/ogg";
    } else if (stringCompare(extension, STRING_LITERAL(".ogv")) == 0) {
        return "video/ogg";
    } else if (stringCompare(extension, STRING_LITERAL(".ogx")) == 0) {
        return "application/ogg";
    } else if (stringCompare(extension, STRING_LITERAL(".opus")) == 0) {
        return "audio/ogg";
    } else if (stringCompare(extension, STRING_LITERAL(".otf")) == 0) {
        return "font/otf";
    } else if (stringCompare(extension, STRING_LITERAL(".pdf")) == 0) {
        return "application/pdf";
    } else if (stringCompare(extension, STRING_LITERAL(".php")) == 0) {
        return "application/x-httpd-php";
    } else if (stringCompare(extension, STRING_LITERAL(".ppt")) == 0) {
        return "application/vnd.ms-powerpoint";
    } else if (stringCompare(extension, STRING_LITERAL(".pptx")) == 0) {
        return "application/vnd.openxmlformats-officedocument.presentationml.presentation";
    } else if (stringCompare(extension, STRING_LITERAL(".rar")) == 0) {
        return "application/vnd.rar";
    } else if (stringCompare(extension, STRING_LITERAL(".rtf")) == 0) {
        return "application/rtf";
    } else if (stringCompare(extension, STRING_LITERAL(".sh")) == 0) {
        return "application/x-sh";
    } else if (stringCompare(extension, STRING_LITERAL(".tar")) == 0) {
        return "application/x-tar";
    } else if (stringCompare(extension, STRING_LITERAL(".tif")) == 0 or
               stringCompare(extension, STRING_LITERAL(".tiff")) == 0) {
        return "image/tiff";
    } else if (stringCompare(extension, STRING_LITERAL(".ts")) == 0) {
        return "video/mp2t";
    } else if (stringCompare(extension, STRING_LITERAL(".ttf")) == 0) {
        return "font/ttf";
    } else if (stringCompare(extension, STRING_LITERAL(".vsd")) == 0) {
        return "application/vnd.visio";
    } else if (stringCompare(extension, STRING_LITERAL(".wav")) == 0) {
        return "audio/wav";
    } else if (stringCompare(extension, STRING_LITERAL(".weba")) == 0) {
        return "audio/webm";
    } else if (stringCompare(extension, STRING_LITERAL(".webm")) == 0) {
        return "video/webm";
    } else if (stringCompare(extension, STRING_LITERAL(".webmanifest")) == 0) {
        return "application/manifest+json";
    } else if (stringCompare(extension, STRING_LITERAL(".webp")) == 0) {
        return "image/webp";
    } else if (stringCompare(extension, STRING_LITERAL(".woff")) == 0) {
        return "font/woff";
    } else if (stringCompare(extension, STRING_LITERAL(".woff2")) == 0) {
        return "font/woff2";
    } else if (stringCompare(extension, STRING_LITERAL(".xhtml")) == 0) {
        return "application/xhtml+xml";
    } else if (stringCompare(extension, STRING_LITERAL(".xls")) == 0) {
        return "application/vnd.ms-excel";
    } else if (stringCompare(extension, STRING_LITERAL(".xlsx")) == 0) {
        return "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet";
    } else if (stringCompare(extension, STRING_LITERAL(".xml")) == 0) {
        return "application/xml";
    } else if (stringCompare(extension, STRING_LITERAL(".xul")) == 0) {
        return "application/vnd.mozilla.xul+xml";
    } else if (stringCompare(extension, STRING_LITERAL(".zip")) == 0) {
        return "application/zip";
    } else if (stringCompare(extension, STRING_LITERAL(".3gp")) == 0) {
        return "video/3gpp";
    } else if (stringCompare(extension, STRING_LITERAL(".3g2")) == 0) {
        return "video/3gpp2";
    } else if (stringCompare(extension, STRING_LITERAL(".7z")) == 0) {
        return "application/x-7z-compressed";
    }
    return "application/octet-stream";
}

static int requestHandler(void* input_ptr) {
    void* CLEAN(free) _socket_ptr_copy = input_ptr;

    RequestHandlerInput input = *(RequestHandlerInput*)input_ptr;

    Arena allocator = {0};
    Result result = SUCCESS;
    int CLEAN(shutdown) socket = input.socket;
    String ip = ZSTRING(input.ip);

    String header = {0};
    String body = {0};

    RequestHeader parsed_header = {0};

    {
        StringBuilder body_builder = {0};
        Arena CLEAN(arenaDestroy) temp_allocator = {0};
        BUBBLE(
            readRequestHeader(&header, &body_builder, socket, &allocator, &temp_allocator),
            "Could not read request header"
        );
        BUBBLE(parseRequestHeader(&parsed_header, header, &allocator), "Could not parse request header");
        BUBBLE(
            readRequestBody(&body, &body_builder, socket, &parsed_header, &allocator, &temp_allocator),
            "Could not read request body"
        );
    }

    for (size_t i = 0; i < parsed_header.path.length - 1; i++) {
        if (parsed_header.path.data[i] == '.' and parsed_header.path.data[i + 1] == '.' and
            (i == 0 or parsed_header.path.data[i - 1] == '/') and
            (i == parsed_header.path.length - 2 or parsed_header.path.data[i + 2] == '/')) {
            String formatted = {0};
            char error_page[] =
                "<!DOCTYPE html>\n"
                "<html>\n"
                "<head>\n"
                "   <meta charset=\"utf-8\" />\n"
                "   <title>Bad Request</title>\n"
                "</head>\n"
                "<body>\n"
                "   <h1>Requested page is malformed</h1>\n"
                "</body>\n"
                "</html>\n";
            BUBBLE(
                stringFormat(
                    &formatted,
                    &allocator,
                    "HTTP/1.1 400 Bad Request\r\n"
                    "Content-Length: %zu\r\n"
                    "Content-Type: text/html\r\n"
                    "\r\n"
                    "%s",

                    sizeof(error_page) - 1,
                    error_page
                ),
                "Could not allocate formatted string"
            );
            send(socket, formatted.data, formatted.length, 0);
            return SUCCESS;
        }
    }

    FILE* CLEAN(fclose) fp = NULL;
    String path = {0};
    BUBBLE(stringFormat(&path, &allocator, ".%.*s", FORMAT(parsed_header.path)), "Could not format path");
    if ((fp = fopen(path.data, "rb"))) {
        fprintf(stderr, "Serving %.*s\n", FORMAT(path));
        fseek(fp, 0, SEEK_END);
        size_t length = ftell(fp);
        fseek(fp, 0, SEEK_SET);

        String file = {0};
        BUBBLE(stringFromFile(&file, &allocator, fp), "Could not read file");

        String formatted = {0};
        BUBBLE(
            stringFormat(
                &formatted,
                &allocator,
                "HTTP/1.1 200 OK\r\n"
                "Content-Length: %zu\r\n"
                "Content-Type: %s\r\n"
                "\r\n"
                "%.*s",

                length,
                getMimeType(path),
                FORMAT(file)
            ),
            "Could not format response header"
        );
        send(socket, formatted.data, formatted.length, 0);
        return SUCCESS;
    }

    String formatted;

    String lua_content_path;
    BUBBLE(
        stringFormat(&lua_content_path, &allocator, ".%.*s.lua", FORMAT(parsed_header.path)),
        "Could not create path"
    );

    String lua_data = LSTRING(NULL, 0);

    if ((fp = fopen(lua_content_path.data, "rb"))) {
        BUBBLE(stringFromFile(&lua_data, &allocator, fp), "Could not read from file");
    }

    if (fp != NULL) {
        lua_State* CLEAN(lua_close) L = luaL_newstate();
        luaL_openlibs(L);

        {
            lua_newtable(L);

            lua_pushlstring(L, ip.data, ip.length);
            lua_setfield(L, -2, "ip");

            lua_pushstring(L, requestMethodToString(parsed_header.method));
            lua_setfield(L, -2, "method");

            lua_pushlstring(L, parsed_header.path.data, parsed_header.path.length);
            lua_setfield(L, -2, "path");

            lua_pushlstring(L, parsed_header.user_agent.data, parsed_header.user_agent.length);
            lua_setfield(L, -2, "user_agent");

            lua_pushlstring(L, body.data, body.length);
            lua_setfield(L, -2, "body");

            {
                lua_newtable(L);
                String queries = parsed_header.queries;
                String query;
                while ((query = stringSplit(&queries, STRING_LITERAL(";"))).data) {
                    String key = stringSplit(&query, STRING_LITERAL("="));
                    String value = query;
                    BUBBLE(stringUrlDecode(&key, key, &allocator), "Could not decode URL query key");
                    BUBBLE(stringUrlDecode(&value, value, &allocator), "Could not decode URL query value");
                    lua_pushlstring(L, key.data, key.length);
                    lua_pushlstring(L, value.data, value.length);
                    lua_rawset(L, -3);
                }
                lua_setfield(L, -2, "queries");
            }

            {
                lua_newtable(L);
                String cookies = parsed_header.cookies;
                String cookie;
                while ((cookie = stringSplit(&cookies, STRING_LITERAL(";"))).data) {
                    String key = stringTrim(stringSplit(&cookie, STRING_LITERAL("=")));
                    String value = stringTrim(cookie);
                    BUBBLE(stringUrlDecode(&key, key, &allocator), "Could not decode cookie key");
                    BUBBLE(stringUrlDecode(&value, value, &allocator), "Could not decode cookie value");
                    lua_pushlstring(L, key.data, key.length);
                    lua_pushlstring(L, value.data, value.length);
                    lua_rawset(L, -3);
                }
                lua_setfield(L, -2, "cookies");
            }

            lua_setglobal(L, "request");
        }

        luaL_dostring(L, lua_data.data);

        {
            lua_getfield(L, -1, "status");
            int status_code = luaL_optint(L, -1, 200);
            lua_pop(L, 1);

            lua_getfield(L, -1, "content_type");
            char const* content_type = luaL_optstring(L, -1, "text/html");
            lua_pop(L, 1);

            String response_body = {0};
            lua_getfield(L, -1, "body");
            response_body.data = (char*)luaL_optlstring(L, -1, "", &response_body.length);
            lua_pop(L, 1);

            String cookies = {0};
            StringBuilder cookie_builder = {0};
            lua_getfield(L, -1, "cookies");
            lua_pushnil(L);
            Arena CLEAN(arenaDestroy) temp_allocator = {0};
            while (lua_next(L, -2) != 0) {
                lua_pushvalue(L, -2);  // prevent key from being changed by lua_to(l)string
                String key = {0};
                key.data = (char*)lua_tolstring(L, -1, &key.length);
                BUBBLE(stringUrlEncode(&key, key, &temp_allocator), "Could not encode cookie key");
                if (lua_type(L, -2) != LUA_TTABLE) {
                    String value = {0};
                    value.data = (char*)lua_tolstring(L, -2, &value.length);
                    BUBBLE(stringUrlEncode(&value, value, &temp_allocator), "Could not encode cookie value");
                    stringAppendf(
                        &cookie_builder,
                        &temp_allocator,
                        "Set-Cookie: %.*s=%.*s\r\n",
                        FORMAT(key),
                        FORMAT(value)
                    );
                } else {
                    lua_getfield(L, -2, "value");
                    String value = {0};
                    value.data = (char*)lua_tolstring(L, -1, &value.length);
                    lua_pop(L, 1);
                    BUBBLE(stringUrlEncode(&value, value, &temp_allocator), "Could not encode cookie value");

                    lua_getfield(L, -2, "max_age");
                    String max_age = {0};
                    if (lua_isnumber(L, -1)) {
                        BUBBLE(
                            stringFormat(&max_age, &temp_allocator, "; Max-Age=%d", lua_tointeger(L, -1)),
                            "Could not set max age of cookie"
                        );
                    }
                    lua_pop(L, 1);

                    lua_getfield(L, -2, "http_only");
                    String http_only = {0};
                    if (lua_toboolean(L, -1)) {
                        BUBBLE(
                            stringFormat(&http_only, &temp_allocator, "; HttpOnly"),
                            "Could not set cookie Http only"
                        );
                    }
                    lua_pop(L, 1);

                    lua_getfield(L, -2, "secure");
                    String secure = {0};
                    if (lua_toboolean(L, -1)) {
                        BUBBLE(stringFormat(&secure, &temp_allocator, "; Secure"), "Could not set cookie secure");
                    }
                    lua_pop(L, 1);

                    lua_getfield(L, -2, "same_site");
                    String same_site = {0};
                    String same_site_value = {0};
                    if (lua_isstring(L, -1) and
                        (same_site_value.data = (char*)lua_tolstring(L, -1, &same_site_value.length))) {
                        char const* value = NULL;
                        if (stringCompare(same_site_value, STRING_LITERAL("strict")) == 0) {
                            value = "Strict";
                        } else if (stringCompare(same_site_value, STRING_LITERAL("lax")) == 0) {
                            value = "Lax";
                        } else if (stringCompare(same_site_value, STRING_LITERAL("none")) == 0) {
                            value = "None";
                            if (secure.data == NULL) {
                                // required
                                BUBBLE(
                                    stringFormat(&secure, &temp_allocator, "; Secure"),
                                    "Could not set cookie secure"
                                );
                            }
                        }
                        if (value != NULL) {
                            BUBBLE(
                                stringFormat(&same_site, &temp_allocator, "; SameSite=%s", value),
                                "Could not set cookie's same site value"
                            )
                        }
                    }
                    lua_pop(L, 1);

                    stringAppendf(
                        &cookie_builder,
                        &temp_allocator,
                        "Set-Cookie: %.*s=%.*s%.*s%.*s%.*s%.*s\r\n",
                        FORMAT(key),
                        FORMAT(value),
                        FORMAT(max_age),
                        FORMAT(http_only),
                        FORMAT(secure),
                        FORMAT(same_site)
                    );
                }
                lua_pop(L, 2);
            }
            stringBuild(&cookies, &cookie_builder, &temp_allocator);

            BUBBLE(
                stringFormat(
                    &formatted,
                    &allocator,
                    "HTTP/1.1 %3d %s\r\n"
                    "Content-Length: %d\r\n"
                    "Content-Type: %s\r\n"
                    "%.*s"
                    "\r\n"
                    "%.*s",
                    status_code,
                    responsePhrase(status_code),
                    response_body.length,
                    content_type,
                    FORMAT(cookies),

                    FORMAT(response_body)
                ),
                "Could not allocate formatted string"
            );
        }

        send(socket, formatted.data, formatted.length, 0);
    } else {
        char error_page[] =
            "<!DOCTYPE html>\n"
            "<html>\n"
            "<head>\n"
            "   <meta charset=\"utf-8\" />\n"
            "   <title>Page not found</title>\n"
            "</head>\n"
            "<body>\n"
            "   <h1>Requested page not found</h1>\n"
            "</body>\n"
            "</html>\n";
        BUBBLE(
            stringFormat(
                &formatted,
                &allocator,
                "HTTP/1.1 404 Not Found\r\n"
                "Content-Length: %zu\r\n"
                "Content-Type: text/html\r\n"
                "\r\n"
                "%s",

                sizeof(error_page) - 1,
                error_page
            ),
            "Could not allocate formatted string"
        );
        send(socket, formatted.data, formatted.length, 0);
    }

    return SUCCESS;
}

static int serverLoop(void* sock) {
    int server_socket = *(int*)sock;
    int client_socket = 0;
    struct sockaddr_in address = {0};
    socklen_t address_length = sizeof(address);

    while (true) {
        ASSERT(LISTEN_ERROR, listen(server_socket, 3) == 0, "%s", strerror(errno));

        if ((client_socket = accept(server_socket, (struct sockaddr*)&address, &address_length)) < 0) {
            if (errno == EINVAL) {
                fprintf(stderr, "Closing...\n");
                break;
            }
            THROW(ACCEPT_ERROR, "%s\n", strerror(errno));
        }

        thrd_t thread;
        RequestHandlerInput* input = malloc(sizeof(*input));
        input->socket = client_socket;
        inet_ntop(AF_INET, &address, input->ip, sizeof(input->ip));
        printf("Connected to %s\n", input->ip);
        thrd_create(&thread, &requestHandler, input);
    }

    close(server_socket);
    return SUCCESS;
}

int main() {
    int CLEAN(close) server_socket = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in address = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(8080),
    };
    socklen_t address_length = sizeof(address);

    ASSERT(SOCKET_ERROR, server_socket >= 0, "%s", strerror(errno));

    {
        int option = 1;
        setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));
    }

    ASSERT(BIND_ERROR, bind(server_socket, (struct sockaddr*)&address, sizeof(address)) >= 0, "%s", strerror(errno))

    thrd_t server_thread;
    thrd_create(&server_thread, &serverLoop, &server_socket);

    while (true) {
        char* line = readline("> ");
        if (line == NULL or strcmp(line, "exit") == 0) {
            fprintf(stderr, "Shutting down the server\n");
            free(line);
            break;
        } else if (strcmp(line, "") == 0) {
        } else {
            fprintf(stderr, "Unrecognized command: \"%s\"\n", line);
        }
        free(line);
    }

    shutdown(server_socket, SHUT_RDWR);

    int server_result;
    thrd_join(server_thread, &server_result);

    return server_result;
}
