// This class contains information and methods relating to the current player, and is created when a new telnet session is started
// There is exactly one player per telnet connection
// The player object is populated with information from the user file and the system on creation
// The player's name is unique for this server.
#ifndef TBDMUD_H_INCLUDED
#define TBDMUD_H_INCLUDED

#include <optional>
#include <map>

namespace tbdmud {

// To hold data about the character currently being used by a player
// (This stores and handles data relevant to the server game)
class character {
    private:
        std::string                   name;
        std::shared_ptr<event_queue>  eq;

    public:
        // Default Constructor
        character() {
            name = "guest";
        };

        // Constructor - Pass in the character's name
        character(std::string n) {
            name = n;
            std::cout << "Character created w/o event queue - name = " << n << std::endl;
        };

        // Constructor - Pass in the character's name and a pointer to the event queue
        character(std::string n, std::shared_ptr<event_queue> e) {
            eq = e;
            name = n;
            std::cout << "Character created w' event queue - name = " << n << std::endl;
        };

        void register_event_queue(std::shared_ptr<event_queue> e) {
            eq = e;
            std::cout << "Character event queue registered" << std::endl;
        }

        std::shared_ptr<event_queue> get_event_queue() {
            return eq;
        }

        std::string get_name() {
            return name;
        }

        void on_tick() {
            
        };

        void on_message(event_scope scope, std::string message) {

        };
};

// To hold data regarding the currently connected player
// (This stores and handles data relevant to the server connection)
class player {
    private:
        std::shared_ptr<character> pc;
        std::string username;
        int         session_id;
        bool        connected = false;   // The player object may exist for a while after a client disconnects, to see if they reconnect
        std::string ip_address;
        int         port;

    public:

        // Default Constructor
        player() {}

        // Constructor - Pass in the session ID and character name (currently the same name as the player)
        player(std::string n, uint sid) {
            username = n;
            session_id = sid;
            std::cout << "Player created (short) - session = " << session_id << ", name = " << n << std::endl;

            // For now, when a player connects we automatically create a character with the same name
            pc = std::shared_ptr<character>(new character(username));
        };

        // Constructor - Pass in the session ID and character name (currently the same name as the player)
        player(std::string n, uint sid, bool c, std::string ip, int p) {
            const char& lastchar = n.back();
            if (lastchar == '\n') boost::trim_right(n);  // If the username has a trailing \n, remove it

            username   = n;
            session_id = sid;
            connected  = c;
            ip_address = ip;
            port       = p;
            std::cout << "Player created (long) - session = " << session_id << ", name = " << n << std::endl;

            // For now, when a player connects we automatically create a character with the same name
            pc = std::shared_ptr<character>(new character(username));
        };

        ~player() {}

        void set_character(std::shared_ptr<character> c) {
            pc = c;
        }

        std::shared_ptr<character> get_character() {
            return pc;
        }

        std::string get_name() {
            return username;
        }

        void set_connected(bool c) {
            connected = c;
        }

        bool is_connected() {
            return connected;
        }

        std::string get_ip() {
            return ip_address;
        }

        int get_port() {
            return port;
        }
};


// A room is the container for all characters and objects in that room, and handles room-wide events
class room {
    private:
        std::string name;
        std::vector<std::shared_ptr<character>> characters;
        std::shared_ptr<event_queue>  eq;

    public:
        // Default Constructor
        room(){
            std::cout << "Room " << name << " constructed w/o event queue" << std::endl;
        }
    
        room(std::string n, std::shared_ptr<event_queue> e) {
            name = n;
            eq = e;
            std::cout << "Room " << name << " constructed with event queue:  " << eq->name << std::endl;
        };

        // Call on_tick() for all the characters in this room
        void on_tick() {
            for (std::shared_ptr<character> pc: characters) {
                pc->on_tick();
            }
        };

        void enter_room(std::shared_ptr<character> c) {
            std::cout << "Character " << c->get_name() << " entered room " << name << std::endl;
            c->register_event_queue(eq);
            characters.push_back(c);
        };  

        void leave_room(std::shared_ptr<character> c) {
            std::vector<std::shared_ptr<character>>::iterator ichar = find(characters.begin(), characters.end(), c);

            if (ichar != characters.end())
            {
                // Remove the character pointer from the vector
                ichar = characters.erase(ichar);
            }
        };
};

// The zone is the container for all the rooms in that zone, and handles zone-wide events
class zone {
    private:
        std::string name;
        std::vector<std::shared_ptr<room>> rooms;
        std::shared_ptr<room> start_room;            // Pointer to the room that new characters start in
        std::shared_ptr<event_queue>  eq;

    public:
        // Default Constructor
        zone() {
            std::cout << "Zone " << name << " constructed w/o event queue" << std::endl;
        };

        zone(std::string n, std::shared_ptr<event_queue> e) {
            name = n;
            eq = e;
            std::cout << "Zone " << name << " constructed with event queue:  " << eq->name << std::endl;
            test_init();
        };

        void test_init() {
            // TODO:  Test rooms until we can read them in from a file
            rooms.push_back(std::shared_ptr<room>(new room("Start", eq)));  // Center room of 9
            rooms.push_back(std::shared_ptr<room>(new room("NW",    eq)));
            rooms.push_back(std::shared_ptr<room>(new room("N",     eq)));
            rooms.push_back(std::shared_ptr<room>(new room("NW",    eq)));
            rooms.push_back(std::shared_ptr<room>(new room("W",     eq)));
            rooms.push_back(std::shared_ptr<room>(new room("E",     eq)));
            rooms.push_back(std::shared_ptr<room>(new room("SW",    eq)));
            rooms.push_back(std::shared_ptr<room>(new room("S",     eq)));
            rooms.push_back(std::shared_ptr<room>(new room("SE",    eq)));
            start_room = rooms.front();
        }

        // Call on_tick() for all the rooms in this zone
        void on_tick() {
            for (std::shared_ptr<room> r : rooms) {
                r->on_tick();
            }
        };

        std::shared_ptr<room> get_start_room() {
            return start_room;
        }
};

}  // end namespace tbdmud

#endif
