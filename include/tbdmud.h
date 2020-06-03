#ifndef TBDMUD_H_INCLUDED
#define TBDMUD_H_INCLUDED

#include <optional>
#include <map>
#include <events.h>

namespace tbdmud {

// This class holds data about the character currently being used by a player in the world
// (The character is created by the World object and then registered with the player
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
            std::cout << "Character " << n << " created" << std::endl;
        };

        // Constructor - Pass in the character's name and a pointer to the event queue
        character(std::string n, std::shared_ptr<event_queue> e) {
            eq = e;
            name = n;
            std::cout << "Character " << n << " created" << std::endl;
        };

        void register_event_queue(std::shared_ptr<event_queue> e) {
            eq = e;
            #ifdef DEBUG
            std::cout << "Character " << name << " " << "event queue registered" << std::endl;
            #endif
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
            zone = z;
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

// This class contains information and methods relating to the current player, and is created when a new telnet session is started
// (This stores and handles data relevant to the server connection)
// There is exactly one player per telnet connection
// The player object is populated with information from the user file and the system on creation
// The player's name is unique for this server.
// The World object will create the character that will be registered to this player
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
// The zone object will create and register the rooms in that zone
class room {
    private:
        std::string name;
        std::vector<std::shared_ptr<character>> characters;
        std::shared_ptr<event_queue>  eq;
        std::map<std::string, std::shared_ptr<room>> exits;  // A collection of exits and the rooms they point to

    public:
        // Default Constructor
        room() {
            std::cout << "Constructed room " << name << std::endl;
        }
    
        room(std::string n, std::shared_ptr<event_queue> e) {
            name = n;
            eq = e;
            std::cout << "Constructed room " << name << std::endl;
        };

        std::string get_name() {
            return name;
        }

        void add_exit(std::string exit_name, std::shared_ptr<room> room_ptr) {
            exits.insert({exit_name, room_ptr});
        }

        // Return a copy of the exits map
        // TODO:  Find a more efficient way of handling this
        std::map<std::string, std::shared_ptr<room>> get_exits() {
            return exits;
        }

        // Get a string of the valid exits for this room
        std::string get_exits_str() {
            std::map<std::string, std::shared_ptr<room>>::iterator e = exits.begin();
            std::string exits_str;

            while (e != exits.end()) {
                exits_str += e->first + " ";
                e++;
            }

            return exits_str;
        }

        // Get a copy of the vector containing the current players in this room
        std::vector<std::shared_ptr<character>> get_characters() {
            return characters;
        }

        // Get a string of the current players in this room
        std::string get_character_str() {
            std::string char_str;

            for (std::shared_ptr<character> c : characters) {
                char_str += "  " + c->get_name() + "\n";
            }

            return char_str;
        }

        // Call on_tick() for all the characters in this room
        void on_tick() {
            for (std::shared_ptr<character> pc: characters) {
                pc->on_tick();
            }
        };

        void enter_room(std::shared_ptr<character> c) {
            #ifdef DEBUG
            std::cout << c->get_name() << " entered room " << name << std::endl;
            #endif
            c->register_event_queue(eq);
            c->set_current_room(name);
            characters.push_back(c);
        };  

        void leave_room(std::shared_ptr<character> c) {
            std::vector<std::shared_ptr<character>>::iterator ichar = find(characters.begin(), characters.end(), c);

            if (ichar != characters.end())
            {
                // Remove the character pointer from the vector
                ichar = characters.erase(ichar);
                c->set_current_room("");
            }
        };
};

// The zone is the container for all the rooms in that zone, and handles zone-wide events
// The world object will create and register each zone
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
            std::cout << "Constructed empty zone" << std::endl;
        };

        zone(std::string n, std::shared_ptr<event_queue> e) {
            name = n;
            eq = e;

            std::cout << "Constructing zone " << name << ":" << std::endl;
            zone_init();
        };

        std::string get_name() {
            return name;
        }

        // Get a copy of the vector containing the current players in this zone
        std::vector<std::shared_ptr<character>> get_characters() {
            return characters;
        }

        void zone_init() {
            // TODO:  Test rooms until we can read them in from a file
            rooms.insert({"Nowhere",   std::shared_ptr<room>(new room("Nowhere",   eq))});  // Default room with no exits - if we end up here there's a problem
            rooms.insert({"Start",     std::shared_ptr<room>(new room("Start",     eq))});  // Center room of 9
            rooms.insert({"NorthEast", std::shared_ptr<room>(new room("NorthEast", eq))});
            rooms.insert({"North",     std::shared_ptr<room>(new room("North",     eq))});
            rooms.insert({"NorthWest", std::shared_ptr<room>(new room("NorthWest", eq))});
            rooms.insert({"West",      std::shared_ptr<room>(new room("West",      eq))});
            rooms.insert({"East",      std::shared_ptr<room>(new room("East",      eq))});
            rooms.insert({"SouthEast", std::shared_ptr<room>(new room("SouthEast", eq))});
            rooms.insert({"South",     std::shared_ptr<room>(new room("South",     eq))});
            rooms.insert({"SouthWest", std::shared_ptr<room>(new room("SouthWest", eq))});
            start_room = rooms["Start"];

            // First create all the rooms, then populate the exits for each room (since they link to each other)
            rooms["Start"]->add_exit("N", rooms["North"]);
            rooms["Start"]->add_exit("S", rooms["South"]);
            rooms["Start"]->add_exit("E", rooms["East"]);
            rooms["Start"]->add_exit("W", rooms["West"]);

            rooms["North"]->add_exit("S", rooms["Start"]);
            rooms["North"]->add_exit("E", rooms["NorthEast"]);
            rooms["North"]->add_exit("W", rooms["NorthWest"]);

            rooms["South"]->add_exit("N", rooms["Start"]);
            rooms["South"]->add_exit("E", rooms["SouthEast"]);
            rooms["South"]->add_exit("W", rooms["SouthWest"]);

            rooms["East"]->add_exit("N", rooms["NorthEast"]);
            rooms["East"]->add_exit("S", rooms["SouthEast"]);
            rooms["East"]->add_exit("W", rooms["Start"]);

            rooms["West"]->add_exit("N", rooms["NorthWest"]);
            rooms["West"]->add_exit("S", rooms["SouthWest"]);
            rooms["West"]->add_exit("E", rooms["Start"]);

            rooms["NorthEast"]->add_exit("S", rooms["East"]);
            rooms["NorthEast"]->add_exit("W", rooms["North"]);

            rooms["NorthWest"]->add_exit("S", rooms["West"]);
            rooms["NorthWest"]->add_exit("E", rooms["North"]);

            rooms["SouthEast"]->add_exit("N", rooms["East"]);
            rooms["SouthEast"]->add_exit("W", rooms["South"]);

            rooms["SouthWest"]->add_exit("N", rooms["West"]);
            rooms["SouthWest"]->add_exit("E", rooms["South"]);
        }

        // Register the character with the zone, and the zone name with the character
        void enter_zone(std::shared_ptr<character> c) {
            std::cout << c->get_name() << " entered zone " << name << std::endl;
            c->register_event_queue(eq);
            c->set_current_zone(name);
            characters.push_back(c);
        };  

        // Remove the character from the zone
        void leave_zone(std::shared_ptr<character> c) {
            std::vector<std::shared_ptr<character>>::iterator ichar = find(characters.begin(), characters.end(), c);

            if (ichar != characters.end())
            {
                // Remove the character pointer from the vector
                ichar = characters.erase(ichar);
                c->set_current_zone("");
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

        // Get a pointer to a room object given the name
        std::shared_ptr<room> get_room(std::string r) {
            return rooms[r];
        }

        // Return the room that is the default starting room for this zone
        std::shared_ptr<room> get_start_room() {
            return start_room;
        }
};

}  // end namespace tbdmud

#endif
