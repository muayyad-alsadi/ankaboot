# Ankaboot
Simple and Fast Linux WebServer for Static Files

This project uses Linux-specific `epoll(2)`, `sendfile(2)` and alike kernel routines, that's why it won't work on BSD or Mac.

No dependencies

# How to build and run

```
make
./ankaboot -d ./public -p 1300
```

