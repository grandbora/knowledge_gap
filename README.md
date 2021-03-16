## Read at https://medium.com/swlh/looking-under-the-hood-http-over-tcp-sockets-952a944c99da

# Compile & Run

Parse an http response
```
clang client.c -o client.out && ./client.out www.wikipedia.com 80
```

Simulate client to server communication
```
clang test_server.c -o server.out && ./server.out 4466
clang client.c -o client.out && ./client.out localhost 4466
```

# Capture
```
sudo tcpdump -i any port 80 -w t1.pkap
sudo tcpdump -i any port 4466 -w t1.pkap
```
