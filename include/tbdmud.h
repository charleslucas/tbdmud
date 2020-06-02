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

    public:
        std::string username;
        int         session_id;
        bool        connected = false;   // The player object may exist for a while after a client disconnects, to see if they reconnect
        std::string ip_address;
        int         port;

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

        std::shared_ptr<character> get_pc() {
            return pc;
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

// There is only one world object per server
// The world is the root/container for all the zones, and handles global events
class world {
    private:
        uint64_t                            current_tick = 0;  // Master clock for the world (in ticks)    
        std::vector<std::shared_ptr<zone>>  zones;
        std::shared_ptr<zone>               start_zone;
        std::shared_ptr<event_queue>        eq;

    public:
        // World Constructor
        world() {
            std::cout << "World Created" << std::endl;
            // Create the event queue with a pointer to the world tick counter
            eq = std::shared_ptr<event_queue>(new event_queue(&current_tick));
            eq->name = "TBDWorld";

            // TODO:  Hard-coded test data until we can read it in from a file
            zones.push_back(std::shared_ptr<zone>(new zone("The Zone", eq)));
            start_zone = zones.front();
        }

        // World Destructor (Vogons?)
        ~world() {}

        // This function should be triggered asynchronously by the server, approximately every second
        // (We're not going to synchronize to real world time)
        void tick() {
            std::cout << "tick " << current_tick << std::endl;
            current_tick++;
            //event_queue.on_tick();

            // Call on_tick() for all the zones in this world
            for (std::shared_ptr<zone> z : zones) {
                z->on_tick();
            }
        };

        // Register a connected character with the room they are starting in
        void register_character(std::shared_ptr<character> c) {
            std::cout << "world:  registering character " << c->get_name() << std::endl; 
            start_zone->get_start_room()->enter_room(c);
        }

        void process_events() {
            std::shared_ptr<event_item> event;

            event = eq->next_event();

            if (event == nullptr) {
                //std::cout << "Got null event" << std::endl; 
            }
            else {
                std::cout << "Got event named " << event->get_name() << std::endl;
            }
        }
};

}  // end namespace tbdmud

#endif
