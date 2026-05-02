#!/bin/bash
set -e

wine tts_server.exe -e data -c data/cert.txt -l :9000 &
TTS_PID=$!

for _ in {1..10}; do
    if (echo > /dev/tcp/127.0.0.1/9000) 2>/dev/null; then break; fi
    sleep 0.5
done || true

nginx -g 'daemon off;' &
NGINX_PID=$!

wait -n $TTS_PID $NGINX_PID
EXIT=$?
kill $TTS_PID $NGINX_PID 2>/dev/null || true
exit $EXIT
