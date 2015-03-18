ringbuf
=======

**ringbuf** is a library of a ring buffer.
*ringbuf.c* and *ringbuf.h* are main program.
*test_ringbuf.c* is a sample program.

A *ringbuf_holder* holds a list of the *ringbuf_entry*. 
And, a *ringbuf_entry* holds a bunch of *ringbuf_item*.
The bunch of *ringbuf_item* is a ring buffer itself.
A *ringbuf_entry* is identified by a key which must be a printable string.

The data structure is like below:

    ~~~~
    holder -- entry -- item -- item -- item -- ... -- item
                |
              entry -- item -- item -- item -- ... -- item
                |
              entry -- item -- item -- item -- ... -- item
    ~~~~

## repository

https://github.com/tanupoo/ringbuf
