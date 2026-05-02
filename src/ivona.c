#define IVONA_BUILDING
#include "ivona.h"

#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

typedef int   (*tts_init_t)(void);
typedef void* (*tts_create_t)(void);
typedef void  (*tts_destroy_t)(void*);
typedef void* (*tts_load_certificate_t)(void*, const char*);
typedef void* (*tts_load_voice_t)(void*, const char*, const char*);
typedef void  (*tts_unload_voice_t)(void*);
typedef void* (*tts_streamer_start_t)(void*, const char*, float);
typedef void* (*tts_streamer_synth_t)(void*, uint32_t);
typedef void  (*tts_streamer_stop_t)(void*);
typedef void  (*tts_wave_delete_t)(void*);
typedef const char* (*tts_errmsg_t)(void);

struct tts_wave { int _pad0; int n_samples; int16_t *pcm; };

#pragma pack(push, 1)
struct wav_hdr {
    char     riff[4]; uint32_t riff_size;
    char     wave[4];
    char     fmt_[4]; uint32_t fmt_size;
    uint16_t fmt, channels;
    uint32_t sample_rate, byte_rate;
    uint16_t block_align, bits_per_sample;
    char     data[4]; uint32_t data_size;
};
#pragma pack(pop)

struct ivona_ctx {
    HMODULE eng_dll;
    void   *engine;
    void   *voice_handle;
    char    voice_name[64];
    char    engine_dir[260];

    tts_init_t              tts_init;
    tts_create_t            tts_create;
    tts_destroy_t           tts_destroy;
    tts_load_certificate_t  tts_load_certificate;
    tts_load_voice_t        tts_load_voice;
    tts_unload_voice_t      tts_unload_voice;
    tts_streamer_start_t    tts_streamer_start;
    tts_streamer_synth_t    tts_streamer_synth;
    tts_streamer_stop_t     tts_streamer_stop;
    tts_wave_delete_t       tts_wave_delete;
    tts_errmsg_t            tts_errmsg;
};

static __thread char g_err[512];

static void set_err(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    vsnprintf(g_err, sizeof(g_err), fmt, ap);
    va_end(ap);
}

IVONA_API const char *ivona_last_error(void) { return g_err; }

IVONA_API void ivona_free(void *p) { free(p); }

static void join_engine_path(const struct ivona_ctx *c, char *out, size_t out_sz, const char *leaf) {
    if (c->engine_dir[0])
        snprintf(out, out_sz, "%s/%s", c->engine_dir, leaf);
    else
        snprintf(out, out_sz, "%s", leaf);
}

IVONA_API ivona_ctx *ivona_open(const char *engine_dir, const char *cert_path) {
    g_err[0] = 0;
    struct ivona_ctx *c = calloc(1, sizeof(*c));
    if (!c) { set_err("oom"); return NULL; }
    if (engine_dir && engine_dir[0])
        snprintf(c->engine_dir, sizeof(c->engine_dir), "%s", engine_dir);

    char path[400];
    join_engine_path(c, path, sizeof(path), "tts_engine.dll");
    c->eng_dll = LoadLibraryA(path);
    if (!c->eng_dll) {
        set_err("LoadLibrary %s: %lu", path, GetLastError());
        free(c); return NULL;
    }

    #define R(n) do { c->n = (n##_t)GetProcAddress(c->eng_dll, #n); \
        if (!c->n) { set_err("missing export %s", #n); goto fail; } } while (0)
    R(tts_init); R(tts_create); R(tts_destroy);
    R(tts_load_certificate); R(tts_load_voice); R(tts_unload_voice);
    R(tts_streamer_start); R(tts_streamer_synth);
    R(tts_streamer_stop); R(tts_wave_delete); R(tts_errmsg);
    #undef R

    c->tts_init();
    c->engine = c->tts_create();
    if (!c->engine) { set_err("tts_create: %s", c->tts_errmsg()); goto fail; }

    FILE *cf = fopen(cert_path, "rb");
    if (!cf) { set_err("cannot open cert '%s'", cert_path); goto fail; }
    fseek(cf, 0, SEEK_END);
    long cl = ftell(cf);
    fseek(cf, 0, SEEK_SET);
    char *cert = malloc(cl + 1);
    if (!cert) { fclose(cf); set_err("oom"); goto fail; }
    fread(cert, 1, cl, cf);
    cert[cl] = 0;
    fclose(cf);
    int ok = c->tts_load_certificate(c->engine, cert) != NULL;
    free(cert);
    if (!ok) { set_err("tts_load_certificate: %s", c->tts_errmsg()); goto fail; }
    return c;

fail:
    if (c->engine) c->tts_destroy(c->engine);
    if (c->eng_dll) FreeLibrary(c->eng_dll);
    free(c);
    return NULL;
}

IVONA_API void ivona_close(ivona_ctx *c) {
    if (!c) return;
    if (c->voice_handle) c->tts_unload_voice(c->voice_handle);
    if (c->engine)       c->tts_destroy(c->engine);
    if (c->eng_dll)      FreeLibrary(c->eng_dll);
    free(c);
}

IVONA_API int ivona_load_voice(ivona_ctx *c, const char *name) {
    g_err[0] = 0;
    if (c->voice_handle && strcmp(c->voice_name, name) == 0) return 1;

    if (c->voice_handle) {
        c->tts_unload_voice(c->voice_handle);
        c->voice_handle = NULL;
        c->voice_name[0] = 0;
    }

    char pat[400], lib[400], vox[400];
    WIN32_FIND_DATAA fd;

    char leaf[300];
    snprintf(leaf, sizeof(leaf), "voice_*%s*.dll", name);
    join_engine_path(c, pat, sizeof(pat), leaf);
    HANDLE h = FindFirstFileA(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) { set_err("no voice DLL matching '%s'", pat); return 0; }
    join_engine_path(c, lib, sizeof(lib), fd.cFileName);
    FindClose(h);

    snprintf(leaf, sizeof(leaf), "vox/vox_*%s*", name);
    join_engine_path(c, pat, sizeof(pat), leaf);
    h = FindFirstFileA(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) { set_err("no vox file matching '%s'", pat); return 0; }
    snprintf(leaf, sizeof(leaf), "vox/%s", fd.cFileName);
    join_engine_path(c, vox, sizeof(vox), leaf);
    FindClose(h);

    c->voice_handle = c->tts_load_voice(c->engine, lib, vox);
    if (!c->voice_handle) { set_err("tts_load_voice: %s", c->tts_errmsg()); return 0; }
    snprintf(c->voice_name, sizeof(c->voice_name), "%s", name);
    return 1;
}

IVONA_API int ivona_synth(ivona_ctx *c, const char *utf8_text,
                          uint8_t **wav_out, uint32_t *wav_size_out) {
    g_err[0] = 0;
    if (!c->voice_handle) { set_err("no voice loaded"); return 0; }

    void *stream = c->tts_streamer_start(c->voice_handle, utf8_text, -1.0f);
    if (!stream) { set_err("tts_streamer_start: %s", c->tts_errmsg()); return 0; }

    // Reserve room for the header up front, then append PCM.
    size_t cap = sizeof(struct wav_hdr) + (1u << 20);
    size_t used = sizeof(struct wav_hdr);
    uint8_t *buf = malloc(cap);
    if (!buf) { c->tts_streamer_stop(stream); set_err("oom"); return 0; }

    for (struct tts_wave *w; (w = c->tts_streamer_synth(stream, IVONA_SAMPLE_RATE / 10));
                              c->tts_wave_delete(w)) {
        if (w->n_samples <= 0 || !w->pcm) continue;
        size_t add = (size_t)w->n_samples * sizeof(int16_t);
        if (used + add > cap) {
            while (used + add > cap) cap *= 2;
            uint8_t *nb = realloc(buf, cap);
            if (!nb) { free(buf); c->tts_streamer_stop(stream); set_err("oom"); return 0; }
            buf = nb;
        }
        memcpy(buf + used, w->pcm, add);
        used += add;
    }
    c->tts_streamer_stop(stream);

    uint32_t data_bytes = (uint32_t)(used - sizeof(struct wav_hdr));
    struct wav_hdr h = {"RIFF", 36 + data_bytes, "WAVE",
                        "fmt ", 16, 1, 1,
                        IVONA_SAMPLE_RATE, IVONA_SAMPLE_RATE * 2, 2, 16,
                        "data", data_bytes};
    memcpy(buf, &h, sizeof(h));
    *wav_out = buf;
    *wav_size_out = (uint32_t)used;
    return 1;
}

IVONA_API int ivona_synth_to_file(ivona_ctx *c, const char *utf8_text, const char *path) {
    uint8_t *wav; uint32_t n;
    if (!ivona_synth(c, utf8_text, &wav, &n)) return 0;
    FILE *f = fopen(path, "wb");
    if (!f) { ivona_free(wav); set_err("cannot open '%s' for writing", path); return 0; }
    size_t w = fwrite(wav, 1, n, f);
    fclose(f);
    ivona_free(wav);
    if (w != n) { set_err("short write to '%s'", path); return 0; }
    return 1;
}
