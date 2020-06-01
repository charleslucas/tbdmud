#ifndef TBDMUD_EVENTS_H_INCLUDED
#define TBDMUD_EVENTS_H_INCLUDED

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

class event_item {
    private:
        // These are overridden by the derivative event class or set by the originating entity
        std::string name = "testname";                                 // Just for logging purposes
        event_scope scope = WORLD;                         // The scope of the effect of this events 
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

        std::string message() {
            // A derivative class can select which message scope to return by default
            return "test";
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

// A priority queue of event wrappers
// The wrapper contains data about when the event should be processed
// The event could be one of a number of derivative event classes
class event_queue {
    private:
        uint64_t*  world_elapsed_ticks;
        uint       event_counter;
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
            
            std::cout << "Add event" << std::endl;
            ec = event_counter;
            //std::cout << "Set ID:  " << event_counter << std::endl; 
            //ew.set_id(event_counter);
            //event_counter++;

            // Set the world-relative tick that this event will trigger on
//            std::cout << "Set STick:  " << *world_elapsed_ticks << " + " << e->rtick() << std::endl; 
//            ew.set_stick(*world_elapsed_ticks + e->rtick());
//
//            std::cout << "Set Event" << std::endl; 
//            ew.set_event(e);
//            
//            std::cout << "Push" << std::endl; 
//            event_pq.push(ew);
        };

        // Return the most current event
        event_wrapper get_event() {
            event_wrapper ew;

            ew = event_pq.top();
            event_pq.pop();

            return ew;
        };

};

}  // end namespace tbdmud

#endif
