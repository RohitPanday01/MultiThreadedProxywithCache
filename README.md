# 🚀 Multithreaded Proxy Server with LRU Cache

A high-performance **multithreaded HTTP proxy server built from scratch in C** that handles concurrent client requests and optimizes repeated requests using an **LRU (Least Recently Used) caching mechanism**.

---

## 📌 Overview

This project was built to gain hands-on experience with:

- Low-level TCP socket programming  
- HTTP request forwarding  
- Multithreading  
- Synchronization primitives  
- Cache design  
- Memory management  
- Concurrent systems programming  

The proxy server accepts client requests, forwards them to remote servers, caches responses, and serves multiple clients concurrently.

---

## ✨ Features

- Multi-threaded request handling using `pthread_create()`
- LRU caching for frequently accessed resources
- Semaphore-based concurrency control
- Mutex-based thread-safe cache operations
- HTTP request parsing and forwarding
- Socket lifecycle management
- Graceful error handling

---

## ⚙️ Tech Stack

| Category | Technology |
|-----------|-------------|
| Language | C |
| Networking | POSIX Sockets |
| Concurrency | Pthreads |
| Synchronization | Mutex Locks, Semaphores |
| Caching | LRU Cache |
| Protocol | HTTP/TCP |

---


## 🔄 Request Lifecycle

### 1. Accept Client Connection
The proxy listens for incoming requests using:

```c
socket()
bind()
listen()
accept()
```

---

### 2. Create New Thread

For every incoming request:

```c
pthread_create()
```

A separate thread is created to handle each client.

---

### 3. Control Concurrency

```c
sem_wait()
sem_post()
```

Semaphores limit the number of simultaneous client connections.

---

### 4. Receive and Parse HTTP Requests

```c
recv()
```

The proxy continuously receives packets until the complete HTTP request is received.

It detects request completion using:

```text
\r\n\r\n
```

---

### 5. Cache Lookup

The server checks whether the requested resource already exists in cache.

### Cache Hit
```text
Client → Cache → Response
```

### Cache Miss
```text
Client → Remote Server → Cache → Response
```

---

### 6. Forward Request to Remote Server

```c
connect()
send()
recv()
```

The request is forwarded to the destination server.

---

### 7. Send Response Back to Client

```c
send()
shutdown()
close()
```

---

# 🧠 LRU Cache Design

The proxy stores frequently requested resources in cache.

When cache size exceeds capacity:

```text
Remove Least Recently Used Entry
```

### Cache Operations

- Add response to cache  
- Search response in cache  
- Update recently used entries  
- Remove least recently used entries  

---

# 🔐 Concurrency Management

## Semaphore

Used to limit active client connections.

```c
sem_wait(&semaphore)
sem_post(&semaphore)
```

---

## Mutex Locks

Used to prevent race conditions during shared cache access.

```c
pthread_mutex_lock()
pthread_mutex_unlock()
```

---

# 🌐 Socket APIs Used

```c
socket()
bind()
listen()
accept()
connect()
send()
recv()
setsockopt()
shutdown()
close()
inet_ntop()
```

---

# 🛠 Challenges Solved

## Partial TCP Reads/Writes
Handled incomplete packet transmission using repeated `recv()` calls.

---

## Thread Safety
Prevented race conditions while multiple threads accessed shared cache.

---

## Memory Management
Handled manual allocation and deallocation in C.

---

## Connection Failures
Implemented graceful handling for disconnected clients and failed requests.

---

# ⚠️ Current Limitations

- Supports only `GET` requests  
- No HTTPS support  
- Fixed cache size  
- Duplicate cache entries possible for simultaneous identical requests  

---

# 🚀 Future Improvements

- Implement thread pool architecture  
- Add HTTPS support  
- Improve cache deduplication  
- Add rate limiting  
- Add request filtering/blocking  
- Build monitoring/logging system  

---



# 📚 Key Learnings

This project helped me gain deeper understanding of:

- Computer Networks  
- Operating Systems  
- Concurrency  
- Caching Systems  
- Memory Management  
- Low-level Systems Programming  

It closely simulates concepts used in real-world infrastructure systems built by companies like Cloudflare, Google, and Amazon.
