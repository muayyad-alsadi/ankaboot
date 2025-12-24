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

## Notes and limits

- Serves files only; no directory listings (might be fixed soon).
- GET only (no HEAD/OPTIONS, though might be fixed soon).
- No range requests, public caching, tag, ..etc (might be fixed soon).
- No TLS termination (put it behind a reverse proxy if needed).
- Basic MIME types included: html, css, js, json, png, jpg, gif, svg, ico, pdf.

## Benchmarks

This tool can be compared with `busybox httpd` and `lighttpd` 

```
busybox httpd -f -h ./public -p 0.0.0.0:8001
echo -e "server.port = 8002\nserver.document-root = \"$PWD/public\"\nserver.max-worker = 1" | lighttpd -D -f -
```

using `ab` and `siege`

```
ab -t 10 -c 100 http://127.0.0.1:8001/index.html
siege -l siege.log -q -b -t 10s -c 100 http://127.0.0.1:8001/index.html
```

but instead of `index.html`, consider a larger file.

