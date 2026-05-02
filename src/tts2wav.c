#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include "ivona.h"

#define FATAL(...) do { fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); return 1; } while (0)

static const char USAGE[] =
    "usage: tts2wav.exe -e <engine_dir> -c <cert_path> <voice> <utf8_text> <out.wav>";

static char *wide_to_utf8(const wchar_t *wstr) {
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, NULL, 0, NULL, NULL);
    char *buf = malloc(len);
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, buf, len, NULL, NULL);
    return buf;
}

int wmain(int argc, wchar_t **argv) {
    const char *engine_dir = NULL, *cert_path = NULL;
    int i = 1;
    while (i < argc && argv[i][0] == L'-' && argv[i][1] && !argv[i][2]) {
        if (i + 1 >= argc)
            FATAL("%s", USAGE);
        if (argv[i][1] == L'e')
            engine_dir = wide_to_utf8(argv[i + 1]);
        else if (argv[i][1] == L'c')
            cert_path = wide_to_utf8(argv[i + 1]);
        else
            break;
        i += 2;
    }
    if (!engine_dir)
        FATAL("missing -e <engine_dir>\n%s", USAGE);
    if (!cert_path)
        FATAL("missing -c <cert_path>\n%s", USAGE);
    if (argc - i != 3)
        FATAL("%s", USAGE);

    char *voice = wide_to_utf8(argv[i]);
    char *text  = wide_to_utf8(argv[i + 1]);
    char *out   = wide_to_utf8(argv[i + 2]);

    ivona_ctx *ctx = ivona_open(engine_dir, cert_path);
    if (!ctx)
        FATAL("ivona_open: %s", ivona_last_error());
    if (!ivona_load_voice(ctx, voice))
        FATAL("%s", ivona_last_error());
    if (!ivona_synth_to_file(ctx, text, out))
        FATAL("%s", ivona_last_error());
    ivona_close(ctx);

    fprintf(stderr, "wrote %s\n", out);
    return 0;
}
