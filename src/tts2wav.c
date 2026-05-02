#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include "ivona.h"

#define DIE(...) do { fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); return 1; } while (0)

static const char USAGE[] =
    "usage: tts2wav.exe -e <engine_dir> -c <cert_path> <voice> <utf8_text> <out.wav>";

static char *w2u(const wchar_t *w) {
    int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, NULL, 0, NULL, NULL);
    char *s = malloc(n);
    WideCharToMultiByte(CP_UTF8, 0, w, -1, s, n, NULL, NULL);
    return s;
}

int wmain(int argc, wchar_t **argv) {
    const char *engine_dir = NULL, *cert_path = NULL;
    int i = 1;
    while (i < argc && argv[i][0] == L'-' && argv[i][1] && !argv[i][2]) {
        if (i + 1 >= argc) DIE("%s", USAGE);
        if (argv[i][1] == L'e')      engine_dir = w2u(argv[i + 1]);
        else if (argv[i][1] == L'c') cert_path  = w2u(argv[i + 1]);
        else break;
        i += 2;
    }
    if (!engine_dir) DIE("missing -e <engine_dir>\n%s", USAGE);
    if (!cert_path)  DIE("missing -c <cert_path>\n%s", USAGE);
    if (argc - i != 3) DIE("%s", USAGE);

    char *voice = w2u(argv[i]);
    char *text  = w2u(argv[i + 1]);
    char *out   = w2u(argv[i + 2]);

    ivona_ctx *ctx = ivona_open(engine_dir, cert_path);
    if (!ctx)                                  DIE("ivona_open: %s", ivona_last_error());
    if (!ivona_load_voice(ctx, voice))         DIE("%s", ivona_last_error());
    if (!ivona_synth_to_file(ctx, text, out))  DIE("%s", ivona_last_error());
    ivona_close(ctx);

    fprintf(stderr, "wrote %s\n", out);
    return 0;
}
