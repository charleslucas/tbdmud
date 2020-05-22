A Simple MUD driver server using the Telnet++ and Server++ libraries from KazDragon, based off his rot13_server telnetpp example:
https://github.com/KazDragon/telnetpp
https://github.com/KazDragon/serverpp


To build:  You will need Python3 pip installed and a virtualenv to run Conan:

...

virtualenv -p /usr/bin/python3 py3env
source py3env/bin/activate
pip install conan
...

You will need the Conan remotes that are registered with the following commands:

```
conan remote add conan-center https://conan.bintray.com
conan remote add bincrafters https://api.bintray.com/conan/bincrafters/public-conan
conan remote add nonstd-lite https://api.bintray.com/conan/martinmoene/nonstd-lite
conan remote add kazdragon-conan https://api.bintray.com/conan/kazdragon/conan-public
```

Conan packages are then installed as following:

```
conan install -s compiler.libcxx=libstdc++11 -s cppstd=14 --build=missing .
```

You can then build with:

```
cmake .
make -j
```

Running the `tbdmud_server` application will then bind to an available port, to which you can connect your Telnet session.