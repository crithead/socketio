# Select, Poll & Epoll Example

The _reader_ (server) binds a socket to some port and listens for connections.
It waits (using select, poll, or epoll) for data to become available on one of
the connections which it either reads and prints or accepts, if on the server
socket.  The _writer_ opens a number of socket connections to the reader's
port, then writes a line of text to a randomly selected connection every few
milliseconds for a number of minutes.

## Reader Methods

The following `-w, --wait-method` are available:

- __select__
- __poll__
- __epoll__

Note __select(2)__ will fail if asked to wait on more than `FD_SETSIZE` file
descriptors, typically 1024. The reader will ignore (and close) file descriptors
with a value of `FD_SETSIZE` or greater.

## Examples

Start a _reader_ that accepts up to 8000 connections on port 11025, using
poll(2) to wait for data on the sockets, for 10 minutes.

```
./reader -p 11025 -s 600 -n 8000 -W poll -v 2>&1 | tee /work/tmp/reader.log
```

Start a _writer_ that opens 8000 connections to port 11025, then writes lines
of text from the file `/work/tmp/pg996.txt` to a random connection every 2 ms
for 9 minutes and 10 seconds.

```
./writer --port 11025 --seconds 550 -n 8000 --delay 2 --text-file /work/tmp/pg996.txt -v 2>&1 | tee /work/tmp/writer.log
./writer --address 192.168.0.13 --port 11025 --seconds 550 -n 8000 --delay 20 --text-file /work/tmp/pg996.txt -v 2>&1 | tee /work/tmp/writer.log
```

