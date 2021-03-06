#ifndef TBDMUD_EVENTS_H_INCLUDED
#define TBDMUD_EVENTS_H_INCLUDED

namespace tbdmud {

// The different scopes of effect that an event can have
enum event_type {
    TYPE_NOT_SET,     // Indicates this value was not set
    NOTICE,           // World broadcast messages - recipients depend on scope
    SPEAK,            // Player speaking event - TELL, SAY, SHOUT or BROADCAST event depending on the event_scope
    MOVE              // Move a character from one room to another
};

// The different scopes of effect that an event can have
// (This may or may not be used depending on the event_type)
enum event_scope {
    SCOPE_NOT_SET,    // Indicates this value was not set
    WORLD,            // Affects everyone active on the server
    ZONE,             // Affects everyone active in the current zone
    LOCAL,            // Affects everyone in the local area (this room and neighboring rooms)
    ROOM,             // Affects everyone active in the current room
    TARGET,           // Affects a specific character/object
    SELF              // Affects the origin character/object
};

class event_item {
    private:
        // These are overridden by the derivative event class or set by the originating entity
        event_type  event_item_type = TYPE_NOT_SET;       // This should be overridden for each created message
        std::string name = "test event";                  // Just for logging purposes
        event_scope scope = SCOPE_NOT_SET;                // The scope of the effect of this events 
        std::map<event_scope, std::string> message_map;   // Potential different messages for each scope
        uint relative_tick = 0;                           // Set if the event should happen N ticks from now
        std::string origin = "";                          // Name of the originating character or room
        std::string target = "";                          // Neme of the target character or room (may be the same as the originator)
        std::string origin_room = "";                     // Name of the originating room (in case of a move)
        std::string target_room = "";                     // Neme of the target room      (in case of a move)

    public:

        void set_rtick(uint r) {
            relative_tick = r;
        };

        uint get_rtick() {
            return relative_tick;
        };

        void set_name(std::string n) {
            name = n;
        }

        std::string get_name() {
            return name;
        }

        void set_type(event_type et) {
            event_item_type = et;
        }

        event_type get_type() {
            return(event_item_type);
        }

        void set_scope(event_scope es) {
            scope = es;
        }

        event_scope get_scope() {
            return(scope);
        }

        // Test if a message has been set for a given scope
        bool has_message(event_scope es) {
            std::map<event_scope, std::string>::iterator mi = message_map.find(es);
            
            return(mi != message_map.end());
        };

        // Set a message for a given scope
        // Note:  This is how you can differentiate between an entry that hasn't been set, vs an entry that was deliberately set to ""
        void set_message(event_scope es, std::string message) {
            message_map.insert({es, message});
            return;
        };

        // Return the message for that scope
        std::string get_message(event_scope es) {
            // The map will return a default-constructed string ("") if the entry has not been set
            return(message_map[es]);
        };

        void set_origin(std::string o) {
            origin = o;
        }

        std::string get_origin() {
            return origin;
        }

        void set_origin_room(std::string o) {
            origin_room = o;
        }

        std::string get_origin_room() {
            return origin_room;
        }

        void set_target(std::string t) {
            target = t;
        }

        std::string get_target() {
            return target;
        }

        void set_target_room(std::string t) {
            target_room = t;
        }

        std::string get_target_room() {
            return target_room;
        }
};

// Wrap a derived event class so we can put different derived event types in the same priority queue
// TODO:  Figure out how to properly handle different derived classes in the same queue, and how to restore derived types without slicing
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

        // Get the world-relative tick when this event will be valid
        uint64_t stick() {
            return scheduled_tick;
        }

        void set_event(std::shared_ptr<event_item> e) {
            event = e;
        }

        std::shared_ptr<event_item> get_event() {
            return(event);
        }

        // Overload operators
        bool operator< (const event_wrapper& other) {
            if (event->get_rtick() == other.event->get_rtick())  {
                if (unique_id < other.unique_id) return true;
                else return false;
            }
            else if (event->get_rtick() < other.event->get_rtick()) return true;
            else return false;
        };

        bool operator> (const event_wrapper& other) {
            if (event->get_rtick() == other.event->get_rtick())  {
                if (unique_id > other.unique_id) return true;
                else return false;
            }
            else if (event->get_rtick() > other.event->get_rtick()) return true;
            else return false;
        }

        friend bool operator< (const event_wrapper& other, const event_wrapper& rhs);
        friend bool operator> (const event_wrapper& other, const event_wrapper& rhs);
};

// Overloaded < operator for priority queue event comparisons
bool operator< (const event_wrapper& lhs, const event_wrapper& rhs) {
    if (lhs.event->get_rtick() == rhs.event->get_rtick())  {
        if (lhs.unique_id < rhs.unique_id) return true;
        else return false;
    }
    else if (lhs.event->get_rtick() < rhs.event->get_rtick()) return true;
    else return false;
};

// Overloaded > operator for priority queue event comparisons
bool operator> (const event_wrapper& lhs, const event_wrapper& rhs) {
    if (lhs.event->get_rtick() == rhs.event->get_rtick())  {
        if (lhs.unique_id > rhs.unique_id) return true;
        else return false;
    }
    else if (lhs.event->get_rtick() > rhs.event->get_rtick()) return true;
    else return false;
};

// A priority queue of event wrappers
// The wrapper contains data about when the event should be processed
// The event could be one of a number of derivative event classes
// TODO:  Figure out how to properly handle different derived classes in the same queue, and how to restore derived types without slicing
class event_queue {
    private:
        uint64_t*                          world_elapsed_ticks;
        uint                               event_counter;
        std::priority_queue<event_wrapper> event_pq;

    public:
        std::string name;  // TODO:  Just for testing

        // Default constructor
        event_queue() {
            event_counter = 0;
        };

        // Class constructor - store a pointer to the world elapsed ticks counter
        event_queue(uint64_t* wet) {
            event_counter = 0;
            world_elapsed_ticks = wet;
        };

        // Provide a shared pointer to an event - events will be sorted by time and then ID as they are added to the priority queue
        void add_event(std::shared_ptr<event_item> e) {
            event_wrapper ew;
            uint ec;
            
            #ifdef DEBUG
            std::cout << "Add event " << e->get_name() << std::endl;
            #endif
            ec = event_counter;

            #ifdef DEBUG
            std::cout << "Set Event ID:  " << event_counter << std::endl; 
            #endif
            ew.set_id(event_counter);
            event_counter++;

            // Set the world-relative tick that this event will trigger on
            #ifdef DEBUG
            std::cout << "Set event system tick to trigger on:  " << *world_elapsed_ticks << " + " << e->get_rtick() << std::endl; 
            #endif
            ew.set_stick(*world_elapsed_ticks + e->get_rtick());

            ew.set_event(e);    // Attach the event object to this wrapper
            event_pq.push(ew);  // Push the event into the priority queue
        };

        // Return the most current event
        std::shared_ptr<event_item> next_event() {
            event_wrapper ew;

            // If the queue has something in it, *AND*
            if (!event_pq.empty()) {
                ew = event_pq.top();

                // If the topmost item in the priority queue has a time equal to or less than now
                if(ew.stick() <= *world_elapsed_ticks) {
                    event_pq.pop();           // Pop it off the queue and return it
                    return ew.get_event();
                }
            }

            // Otherwise, if we didn't pop an event off the queue, just return nullptr
            return nullptr;
        };

};

}  // end namespace tbdmud

#endif
