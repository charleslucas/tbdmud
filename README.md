A Simple MUD driver server using the Boost:ASIO libraries, originally based off this Asychronous TCP Server tutorial:
https://dens.website/tutorials/cpp-asio/async-tcp-server

All source code and docs are available on GitHub:
https://github.com/charleslucas/tbdmud


To build:
  install boost libraries
  make

Running the `tbdmud_server` application will then bind to a specified port (currently 15001), to which you can connect Telnet sessions.
