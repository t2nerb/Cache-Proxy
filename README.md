Cache Proxy
===========

HTTP proxy that supports GET requests. The proxy caches all get requests with an expiration timeout set via command line arguments. 

Design
-----------

The timeline of events is as follows: 

```
   Create socket
         |
         V
Bind to localhost port
         |
         V
   Listen on port 
         |
         V
Accept multiple connections
         |
         V 
  Fork a new process
         |
         V 
   Handle request 
```

On every request made from the client's browser, the proxy uses `fork()` to create a child process to handle the request. I chose to do this to optimize use of multiple cores. 

When the proxy receives a request, the first thing it does is check if the request hostname is blocked. Blocked hostnames should be in the project's root directory with filename `blocklist.txt`. 

If the hostname isn't blocked, the proxy then attempts to perform a DNS lookup for the hostname and checks if hostname is in the hostname-IP cache. Again, the proxy checks if the IP address is blocked in the same file. If not, then it proceeds. 

Next, a md5sum is computed on the first line of the request data. This line contains all information regarding what particular data is intended to be retrieved. All GET requests for data are guaranteed to have a unique first line in the header.

With the header hash value, the proxy checks to see if there is a file in the cache with the filename hash value. If there is, the proxy serves file straight from the cache if it is less than the timeout expiration. Otherwise, the proxy makes a connection with the server IP and forwards the client's request. Once the requested data is received, it is forwarded straight to the client and is written to a file with the hash value as filename. 


Setup
-----------

A makefile is provided to aid in compilation, simply execute `make` in the project's root directory. 

To clean the directory from temporary files, use `make clean`. 

Usage
-----------

To start the proxy use the command: `./proxy <port_number>` 

This starts the proxy and listens on localhost's port 8000. 

To start the proxy with specified cache expiration: `./proxy <port_number> -t <timeout_value>`

The timeout value should be a number which will indicate the number of seconds that the cached file should be allowed to be served. 
