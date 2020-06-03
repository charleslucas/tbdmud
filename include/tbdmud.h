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
        std::string zone;  // The current zone that the player is in
        std::string room;  // The current room that the player is in

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

        void set_name(std::string n) {
            name = n;
        }

        std::string get_name() {
            return name;
        }

        void set_current_zone(std::string z) {
            room = z;
        }

        std::string get_current_zone() {
            return zone;
        }
        
        void set_current_room(std::string r) {
            room = r;
        }

        std::string get_current_room() {
            return room;
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
        std::map<std::string, std::shared_ptr<room>> exits;  // A collection of exits and the rooms they point to

    public:
        // Default Constructor
        room() {
            std::cout << "Room " << name << " constructed w/o event queue" << std::endl;
        }
    
        room(std::string n, std::shared_ptr<event_queue> e) {
            name = n;
            eq = e;
            std::cout << "Room " << name << " constructed with event queue:  " << eq->name << std::endl;
        };

        std::string get_name() {
            return name;
        }

        void add_exit(std::string exit_name, std::shared_ptr<room> roomptr) {
            exits.insert({exit_name, roomptr});
        }

        std::string get_valid_exits() {
            std::map<std::string, std::shared_ptr<room>>::iterator e = exits.begin();
            std::string exits_str;

            while (e != exits.end()) {
                exits_str += e->first;
                e++;
            }

            return exits_str;
        }

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
        std::map<std::string, std::shared_ptr<room>> rooms;
        std::shared_ptr<room> start_room;            // Pointer to the room that new characters start in
        std::vector<std::shared_ptr<character>> characters;
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
            zone_init();
        };

        void zone_init() {
            // TODO:  Test rooms until we can read them in from a file
            rooms.insert({"Start", std::shared_ptr<room>(new room("Start", eq))});  // Center room of 9
            rooms.insert({"NE",    std::shared_ptr<room>(new room("NE",    eq))});
            rooms.insert({"N",     std::shared_ptr<room>(new room("N",     eq))});
            rooms.insert({"NW",    std::shared_ptr<room>(new room("NW",    eq))});
            rooms.insert({"W",     std::shared_ptr<room>(new room("W",     eq))});
            rooms.insert({"E",     std::shared_ptr<room>(new room("E",     eq))});
            rooms.insert({"SE",    std::shared_ptr<room>(new room("SE",    eq))});
            rooms.insert({"S",     std::shared_ptr<room>(new room("S",     eq))});
            rooms.insert({"SW",    std::shared_ptr<room>(new room("SW",    eq))});
            start_room = rooms["Start"];

            // First create all the rooms, then populate the exits for each room (since they link to each other)
            rooms["Start"]->add_exit("N", rooms["N"]);
            rooms["Start"]->add_exit("S", rooms["S"]);
            rooms["Start"]->add_exit("E", rooms["E"]);
            rooms["Start"]->add_exit("W", rooms["W"]);

            rooms["N"]->add_exit("S", rooms["Start"]);
            rooms["N"]->add_exit("E", rooms["NE"]);
            rooms["N"]->add_exit("W", rooms["NW"]);

            rooms["S"]->add_exit("N", rooms["Start"]);
            rooms["S"]->add_exit("E", rooms["SE"]);
            rooms["S"]->add_exit("W", rooms["SW"]);

            rooms["E"]->add_exit("N", rooms["NE"]);
            rooms["E"]->add_exit("S", rooms["SE"]);
            rooms["E"]->add_exit("W", rooms["Start"]);

            rooms["W"]->add_exit("N", rooms["NW"]);
            rooms["W"]->add_exit("S", rooms["SW"]);
            rooms["W"]->add_exit("E", rooms["Start"]);

            rooms["NE"]->add_exit("S", rooms["E"]);
            rooms["NE"]->add_exit("W", rooms["N"]);

            rooms["NW"]->add_exit("S", rooms["W"]);
            rooms["NW"]->add_exit("E", rooms["N"]);

            rooms["SE"]->add_exit("N", rooms["E"]);
            rooms["SE"]->add_exit("W", rooms["S"]);

            rooms["SW"]->add_exit("N", rooms["W"]);
            rooms["SW"]->add_exit("E", rooms["S"]);
            std::cout << "Finished creating rooms" << std::endl;
        }

        // Register the character with the zone
        void enter_zone(std::shared_ptr<character> c) {
            std::cout << "Character " << c->get_name() << " entered zone " << name << std::endl;
            c->register_event_queue(eq);
            characters.push_back(c);
        };  

        // Remove the character from the zone
        void leave_zone(std::shared_ptr<character> c) {
            std::vector<std::shared_ptr<character>>::iterator ichar = find(characters.begin(), characters.end(), c);

            if (ichar != characters.end())
            {
                // Remove the character pointer from the vector
                ichar = characters.erase(ichar);
            }
        };

        // Call on_tick() for all the rooms in this zone
        void on_tick() {
            std::map<std::string, std::shared_ptr<room>>::iterator r = rooms.begin();

            while (r != rooms.end())
        	{
                r->second->on_tick();
                r++;
            }
        };

        std::shared_ptr<room> get_room(std::string r) {
            std::cout << "Getting room" << r << std::endl;
            return rooms[r];
        }

        std::shared_ptr<room> get_start_room() {
            std::cout << "Getting start room" << std::endl;
            return start_room;
        }
};

}  // end namespace tbdmud

#endif
