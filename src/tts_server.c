#include <fcgiapp.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "ivona.h"

#define DIE(...) do { fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); return 1; } while (0)

static void respond(FCGX_Request *r, int status, const char *content_type,
                    const void *body, size_t len) {
    if (status != 200) FCGX_FPrintF(r->out, "Status: %d\r\n", status);
    FCGX_FPrintF(r->out, "Content-Type: %s\r\n\r\n", content_type);
    if (body && len) FCGX_PutStr((const char *)body, (int)len, r->out);
}
static void respond_err(FCGX_Request *r, int status, const char *msg) {
    respond(r, status, "text/plain; charset=utf-8", msg, strlen(msg));
}

static const char USAGE[] =
    "usage: tts_server.exe -e <engine_dir> -c <cert_path> [-l <addr:port>]";

int main(int argc, char **argv) {
    const char *engine_dir = NULL, *cert_path = NULL, *listen_addr = ":9000";
    int i = 1;
    while (i < argc && argv[i][0] == '-' && argv[i][1] && !argv[i][2]) {
        if (i + 1 >= argc) DIE("%s", USAGE);
        char f = argv[i][1];
        if      (f == 'e') engine_dir  = argv[i + 1];
        else if (f == 'c') cert_path   = argv[i + 1];
        else if (f == 'l') listen_addr = argv[i + 1];
        else break;
        i += 2;
    }
    if (!engine_dir) DIE("missing -e <engine_dir>\n%s", USAGE);
    if (!cert_path)  DIE("missing -c <cert_path>\n%s", USAGE);
    if (i != argc)   DIE("%s", USAGE);

    ivona_ctx *ctx = ivona_open(engine_dir, cert_path);
    if (!ctx) DIE("ivona_open: %s", ivona_last_error());

    if (FCGX_Init() != 0) DIE("FCGX_Init failed");
    int sock = FCGX_OpenSocket(listen_addr, 16);
    if (sock < 0) DIE("FCGX_OpenSocket %s failed", listen_addr);

    fprintf(stderr, "tts_server: ready (engine=%s cert=%s listen=%s)\n",
            engine_dir, cert_path, listen_addr);

    FCGX_Request req;
    FCGX_InitRequest(&req, sock, 0);

    while (FCGX_Accept_r(&req) >= 0) {
        const char *voice_raw = FCGX_GetParam("VOICE",          req.envp);
        const char *cl_str    = FCGX_GetParam("CONTENT_LENGTH", req.envp);

        if (!voice_raw || !*voice_raw) {
            respond_err(&req, 400, "missing 'voice' query parameter\n");
            FCGX_Finish_r(&req); continue;
        }
        long cl = cl_str ? atol(cl_str) : 0;
        if (cl <= 0 || cl > (1L << 20)) {
            respond_err(&req, 400, "missing or unreasonable Content-Length\n");
            FCGX_Finish_r(&req); continue;
        }

        char *text = malloc((size_t)cl + 1);
        if (!text) { respond_err(&req, 500, "oom\n"); FCGX_Finish_r(&req); continue; }
        int n = FCGX_GetStr(text, (int)cl, req.in);
        if (n != cl) {
            respond_err(&req, 400, "short body\n");
            free(text); FCGX_Finish_r(&req); continue;
        }
        text[cl] = 0;

        if (!ivona_load_voice(ctx, voice_raw)) {
            char buf[512];
            snprintf(buf, sizeof(buf), "%s\n", ivona_last_error());
            respond_err(&req, 400, buf);
            free(text); FCGX_Finish_r(&req); continue;
        }
        uint8_t *wav; uint32_t wav_size;
        if (!ivona_synth(ctx, text, &wav, &wav_size)) {
            char buf[512];
            snprintf(buf, sizeof(buf), "%s\n", ivona_last_error());
            respond_err(&req, 500, buf);
            free(text); FCGX_Finish_r(&req); continue;
        }
        respond(&req, 200, "audio/wav", wav, wav_size);
        ivona_free(wav);

        const char *uri = FCGX_GetParam("REQUEST_URI", req.envp);
        fprintf(stderr, "tts_server: %s → %u bytes\n", uri ? uri : "?", wav_size);

        free(text);
        FCGX_Finish_r(&req);
    }

    ivona_close(ctx);
    return 0;
}
