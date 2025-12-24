# Ankaboot
Simple, fast Linux static file server that leans on the kernel.

![Ankaboot](ankaboot.png)

Ankaboot is a tiny, zero-dependency HTTP server built around `epoll(2)` and
`sendfile(2)` to serve static files fast with minimal copies. It is deliberately small
and focused so it can be dropped into containers, edge boxes, or build artifacts
without drag.

Note: Linux only. It uses `epoll(2)`/`sendfile(2)` and does not run on BSD or macOS.

## Why it feels fast
- Edge-triggered `epoll(7)` for high concurrency.
- `sendfile(2)` for zero-copy file transfer.
- Single-purpose code path: GET static files only.
- Zero dependencies, single binary.

## Quick start
```
make
./ankaboot -d ./public -p 1300
```

Defaults:
- `-d`: `./`
- `-p`: `8080`

Try it:
```
curl http://localhost:1300/index.html
```

## Usage
```
./ankaboot [-d htdocs_dir] [-p port_num]
```

## Docker / Podman
```
docker build -t ankaboot .
docker run --rm -ti --net host -u $UID -v $PWD/public:/app/public ankaboot
```

On MacOS, run with:
```
docker run --rm -ti -p 8080:8080 -u $UID -v $PWD/public:/app/public ankaboot
```

## Notes and limits
- Serves files only; no directory listings.
- GET only (no HEAD/OPTIONS).
- No TLS termination (put it behind a reverse proxy if needed).
- Basic MIME types included: html, css, js, json, png, jpg, gif, svg, ico, pdf.
