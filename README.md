# ret26
Reflection Enabled Toolkit for C++26

## What's this?

Holding a talk about the C++ 26 reflection feature got out of hand in terms of
implementing a full toolkit useful for writing networked applications enabled
by the new reflection feature.

Parts:
- json26.hpp           : reflection enabled json (free-standing)
- sql26/*.hpp          : reflection enabled Linq-like SQL access.
- ret26/*.hpp          : convenience helpers and tools like async tasks
- net26/(http|tcp).hpp : http and in general tcp servers (port of the old net11)
