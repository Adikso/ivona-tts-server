FROM debian:bookworm-slim AS build

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        ca-certificates git autoconf automake libtool make \
        gcc-mingw-w64-x86-64 mingw-w64-x86-64-dev pkg-config && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY Makefile ./
COPY src ./src
COPY scripts ./scripts
RUN make

FROM debian:bookworm-slim AS runtime

ENV DEBIAN_FRONTEND=noninteractive \
    WINEDEBUG=-all \
    WINEDLLOVERRIDES="mscoree=;mshtml="

RUN apt-get update && \
    apt-get install -y --no-install-recommends wine wine64 nginx && \
    rm -rf /var/lib/apt/lists/* && \
    useradd -m -u 1000 tts && \
    mkdir -p /var/lib/nginx /var/log/nginx /tmp/nginx-body && \
    chown -R tts:tts /var/lib/nginx /var/log/nginx /tmp/nginx-body

USER tts
WORKDIR /home/tts/app
ENV WINEPREFIX=/home/tts/.wine
RUN wineboot --init >/dev/null 2>&1 && wineserver --wait

COPY --chown=tts:tts --from=build \
     /src/build/ivona.dll /src/build/tts2wav.exe /src/build/tts_server.exe ./
COPY --chown=tts:tts data ./data
COPY --chown=tts:tts docker/entrypoint.sh ./entrypoint.sh
COPY --chown=tts:tts docker/nginx.conf /etc/nginx/nginx.conf
RUN chmod +x ./entrypoint.sh

EXPOSE 8080
CMD ["./entrypoint.sh"]
