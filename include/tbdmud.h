// This class contains information and methods relating to the current player, and is created when a new telnet session is started
// There is exactly one player per telnet connection
// The player object is populated with information from the user file and the system on creation
// The player's name is unique for this server.
#ifndef TBDMUD_H_INCLUDED
#define TBDMUD_H_INCLUDED

#include <optional>
#include <map>

namespace tbdmud {

// The different scopes of effect that an event can have
enum event_scope {
    WORLD,            // Affects everyone active on the server
    ZONE,             // Affects everyone active in the current zone
    LOCAL,            // Affects everyone in the local area (this room and neighboring rooms)
    ROOM,             // Affects everyone active in the current room
    TARGET,           // Affects a specific character/object
    SELF              // Affects the origin character/object
};

// To hold data about the character currently being used by a player
// (This stores and handles data relevant to the server game)
class character {
    private:
        std::string                   name;

    public:
        //std::optional<tbdmud::player> player;  // The player currently playing this character (it is possible we can have characters driven by NPC, without a player)

        // Default Constructor
        character() {
            name = "guest";
        };

        // Constructor - Pass in the character's name and a pointer to the event queue
        character(std::string n) {
            name = n;
        };

        void on_tick() {
            
        };

        void on_message(event_scope scope, std::string message) {

        };
};

// To hold data regarding the currently connected player
// (This stores and handles data relevant to the server connection)
class player {
    private:
    public:
        int         session_id;
        bool        connected = false;   // The player object may exist for a while after a client disconnects, to see if they reconnect

        std::string username;
        std::string ip_address;
        int         port;

        std::optional<tbdmud::character> character;

        // Default Constructor
        player() {

        };

        // Constructor - Pass in the session ID
        player(int session) {
            session_id = session;
        };

        ~player() {
            
        }
};

class event_item {
    private:
        // These are overridden by the derivative event class or set by the originating entity
        std::string name;                                 // Just for logging purposes
        event_scope scope = SELF;                         // The scope of the effect of this events 
        std::map<event_scope, std::string> message_map;   // Potential different messages for each scope
        uint relative_tick = 0;                           // Set if the event should happen N ticks from now

    public:

        uint rtick() {
            return relative_tick;
        };

        bool has_message(event_scope es) {
            return(false);
        };

        // Return a particular message given the scope
        std::string message(event_scope es) {
            //TODO:  check that the scope actually exists
            return message_map[es];
        };

        virtual std::string message() {
            // A derivative class can select which message scope to return by default
            return "";
        };

};

class tell_event : event_item {
    private:
    public:

    void set_message(std::string m) {
        // Insert into the target scope of the message map

    }
};

class say_event : event_item {
    private:
    public:

    void set_message(std::string m) {
        // Insert into the room scope of the message map

    }
};

class shout_event : event_item {
    private:
    public:

    void set_message(std::string m) {
        // Insert into the zone scope of the message map
        
    }
};

class broadcast_event : event_item {
    private:
    public:

    void set_message(std::string m) {
        // Insert into the global scope of the message map
        
    }
};

class message_event : event_item {
    private:
        std::string message;
    public:
};

class move_event : event_item {
    private:
        std::string message;
    public:
};

// Wrap a derived event class so we can put different derived event types in the same priority queue
class event_wrapper {
    private:
        // These are set by the event queue
        uint      unique_id = 0;                               // The unique sequential id for this event (set by the event queue)
        uint64_t scheduled_tick = 0;                          // The tick (server time) when this event is scheduled to happen (0 = immediate)
        std::shared_ptr<event_item> event;

    public:

        event_wrapper() {}

        event_wrapper(std::shared_ptr<event_item> e) {
            event = e;
        };

        void set_id(uint i) {
            unique_id = i;
        };

        void set_stick(uint64_t s) {
            scheduled_tick = s;
        }

        uint id() {
            return unique_id;
        }

        uint64_t stick() {
            return scheduled_tick;
        }

        void set_event(std::shared_ptr<event_item> e) {
            event = e;
        }

        // Overload operators
        bool operator< (const event_wrapper& other) {
            if (event->rtick() == other.event->rtick())  {
                if (unique_id < other.unique_id) return true;
                else return false;
            }
            else if (event->rtick() < other.event->rtick()) return true;
            else return false;
        };

        bool operator> (const event_wrapper& other) {
            if (event->rtick() == other.event->rtick())  {
                if (unique_id > other.unique_id) return true;
                else return false;
            }
            else if (event->rtick() > other.event->rtick()) return true;
            else return false;
        }

        friend bool operator< (const event_wrapper& other, const event_wrapper& rhs);
        friend bool operator> (const event_wrapper& other, const event_wrapper& rhs);
};

// Overloaded < operator for priority queue event comparisons
bool operator< (const event_wrapper& lhs, const event_wrapper& rhs) {
    if (lhs.event->rtick() == rhs.event->rtick())  {
        if (lhs.unique_id < rhs.unique_id) return true;
        else return false;
    }
    else if (lhs.event->rtick() < rhs.event->rtick()) return true;
    else return false;
};

// Overloaded > operator for priority queue event comparisons
bool operator> (const event_wrapper& lhs, const event_wrapper& rhs) {
    if (lhs.event->rtick() == rhs.event->rtick())  {
        if (lhs.unique_id > rhs.unique_id) return true;
        else return false;
    }
    else if (lhs.event->rtick() > rhs.event->rtick()) return true;
    else return false;
};

// The comparison function to be used with the priority queue in the event queue class
// Should sort events first by the event's time field (0 = immediate), and then by the event ID
// so that if multiple events happen at the same tick (or immediately), they will be processed in the order entered
//struct event_queue_compare {
//    bool operator()(const pair<event,int> &a, const pair<event,int> &b) {
//        return a.second > b.second;
//    };
//};

class event_queue {
    private:
        uint64_t*  world_elapsed_ticks;
        uint       event_counter = 0;
        std::priority_queue<event_wrapper> event_pq;

    public:

        // Default constructor
        event_queue() {};

        // Class constructor - store a pointer to the world elapsed ticks counter
        event_queue(uint64_t* wet) {
            world_elapsed_ticks = wet;
        };

        // Provide a shared pointer to an event - events will be sorted by time and then ID as they are added to the priority queue
        void add_event(event_item e) {
            event_wrapper ew;

            ew.set_id(event_counter);
            event_counter++;

            // Set the world-relative tick that this event will trigger on
            ew.set_stick(*world_elapsed_ticks + e.rtick());

            ew.set_event(std::make_shared<event_item>(e));
            
            event_pq.push(ew);
        };

        // Return the most current event
        event_wrapper get_event() {
            event_wrapper ew;

            ew = event_pq.top();
            event_pq.pop();

            return ew;
        };

};

// A room is the container for all characters and objects in that room, and handles room-wide events
class room {
    private:
        std::vector<character> characters;

    public:
        // Call on_tick() for all the player characters in this room
        void on_tick() {
            for (character pc: characters) {
                pc.on_tick();
            }
        };
};

// The zone is the container for all the rooms in that zone, and handles zone-wide events
class zone {
    private:
        std::vector <room> rooms;

    public:
        // Call on_tick() for all the rooms in this zone
        void on_tick() {
            for (room r : rooms) {
                r.on_tick();
            }
        };
};

// There is only ever one world object per server
// The world is the root/container for all the zones, and handles global events
class world {
    private:
        uint64_t           current_tick = 0;  // Master clock for the world (in ticks)    
        std::vector<zone>  zones;
        event_queue        eq;

    public:

        // Default constructor
        world() : eq(&current_tick) {}

        // Destructor
        ~world() {}

        // This function should be trigger asynchronously by the server, approximately every second
        // (We're not going to synchronize to real world time)
        void tick() {
            current_tick++;
            //event_queue.on_tick();

            // Call on_tick() for all the zones in this world
            for (zone z : zones) {
                z.on_tick();
            }
        };
};

}  // end namespace tbdmud

#endif
