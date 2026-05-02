#ifndef IVONA_H
#define IVONA_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef IVONA_BUILDING
# define IVONA_API __declspec(dllexport)
#else
# define IVONA_API __declspec(dllimport)
#endif

#define IVONA_SAMPLE_RATE 22050  // Hz, mono, 16-bit PCM

typedef struct ivona_ctx ivona_ctx;

IVONA_API ivona_ctx  *ivona_open(const char *engine_dir, const char *cert_path);
IVONA_API void        ivona_close(ivona_ctx *ctx);
IVONA_API int         ivona_load_voice(ivona_ctx *ctx, const char *voice_name);
IVONA_API int         ivona_synth(ivona_ctx *ctx, const char *utf8_text,
                                  uint8_t **wav_out, uint32_t *wav_size_out);
IVONA_API int         ivona_synth_to_file(ivona_ctx *ctx, const char *utf8_text,
                                          const char *wav_path);
IVONA_API void        ivona_free(void *p);
IVONA_API const char *ivona_last_error(void);

#ifdef __cplusplus
}
#endif

#endif
