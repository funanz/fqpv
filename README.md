# Fast? Pipe Viewer

    $ make
    $ yes | ./fqpv > /dev/null
    138.21 GiB  0:00:11.000 [ 12.57 GiB/s] <splice>

# Results
|Test command|fqpv|pv||
|:---|---:|---:|:---|
|yes \| pv > /dev/null|12.57 GiB/s|10.6 GiB/s|
|yes \| pv \| cat > /dev/null|17.78 GiB/s|2.18 GiB/s|what?|
|yes \| pv \| zstd > /dev/null|6.81 GiB/s|2.10 GiB/s|
|pv /dev/zero > /dev/null|35.69 GiB/s|27.8 GiB/s|
|pv /dev/zero \| cat > /dev/null|10.72 GiB/s|3.64 GiB/s|
|pv /dev/zero \| zstd > /dev/null|4.64 GiB/s|3.09 GiB/s|

# Conclusion
Just increese the pipe size.

    fcntl(fd, F_SETPIPE_SZ, 1048576); // on linux
