# Ivona TTS server

Small wrapper around the Ivona x64 TTS engine. The vendor DLLs run under Wine on Linux, fronted either by an HTTP server or a CLI.

- **HTTP**: nginx -> FastCGI responder. Post text, get a WAV.
- **CLI**: `tts2wav.exe` synth one utterance to a WAV file.

Both share `ivona.dll`, a thin C wrapper over `tts_engine.dll`.

## Run with Docker

```sh
docker build -f docker/Dockerfile -t ivona-tts .
docker run -d -p 8080:8080 ivona-tts

curl --data-binary "Witaj jestem Jacek, jestem głosem syntezatora mowy Ivona" 'http://localhost:8080/synth?voice=jacek' -o out.wav
```

Voices auto-discovered from `data/voice_*.dll`. Currently: `jacek`, `ewa`, `jan`, `maja`.

## Build natively (no Docker)

Requires `x86_64-w64-mingw32-gcc`, `make`, `wine`, plus
`git autoconf automake libtool` for the libfcgi cross-compile.

```sh
# ivona.dll + tts2wav.exe + tts_server.exe → build/
# auto-fetches and cross-compiles libfcgi on first run
make             
wine ./build/tts2wav.exe -e data -c data/cert.txt jacek "Cześć" out.wav
```

## API

```
POST /synth?voice=<name>
Body: raw UTF-8 text

→ 200 audio/wav         on success
→ 400/500 text/plain    on error
```

Output is always 22050 Hz mono 16-bit PCM WAV.
