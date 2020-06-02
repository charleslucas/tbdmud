#ifndef TBDMUD_WORLD_H_INCLUDED
#define TBDMUD_WORLD_H_INCLUDED

namespace tbdmud {

// There is only one world object per server
// The world is the root/container for all the zones, and handles global events
class world {
    private:
        std::map<std::string, session*> char_to_client_map;  // To refer messages to characters back to the associated session
        uint64_t                            current_tick = 0;  // Master clock for the world (in ticks)    
        std::vector<std::shared_ptr<zone>>  zones;
        std::shared_ptr<zone>               start_zone;
        std::shared_ptr<event_queue>        eq;

    public:
        // World Constructor
        // In the beginning....
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

        // Create a new character and put them in the starting room
        std::shared_ptr<character> create_character(session* client, std::string name) {
            std::cout << "world:  creating new character " << std::endl;
            std::shared_ptr<character> c = std::shared_ptr<character>(new character(name));

            char_to_client_map.insert({name, client});
            start_zone->get_start_room()->enter_room(c);

            return c;
        }

        // Register an existing character and put them in the starting room
        // TODO:  Put them in the room they logged out in
        void register_character(session* client, std::shared_ptr<character> c) {
            std::cout << "world:  registering character " << c->get_name() << std::endl;

            char_to_client_map.insert({c->get_name(), client});
            start_zone->get_start_room()->enter_room(c);
        }

        // Delete a character - remove them from the room they are in and other cleanup
        void remove_character(std::string character_name) {
            std::cout << "world:  removing character " << character_name << std::endl;

            char_to_client_map.erase(character_name);
            //start_zone->get_start_room()->enter_room(c);
            // TODO:  Remove from room
        }

        void process_events() {
            std::shared_ptr<event_item> event;

            event = eq->next_event();

            if (event == nullptr) {
                //std::cout << "Got null event" << std::endl; 
            }
            else {
                switch(event->get_type()) {
                    case SPEAK:
                        switch(event->get_scope()) {
                            case TARGET:
                                std::cout << "Got TELL to " << event->get_target() << ":  " << event->get_message(TARGET);
                                break;
                            case ROOM:
                                std::cout << "Got SAY event:  " << event->get_message(ROOM) << std::endl;
                                break;
                            case ZONE:
                                std::cout << "Got SHOUT event:  " << event->get_message(ZONE) << std::endl;
                                break;
                            case WORLD:
                                std::cout << "Got BROADCAST event:  " << event->get_message(WORLD) << std::endl;
                                break;
                            default:
                                std::cout << "Got unknown SPEAK event:  " << event->get_name() << std::endl;
                                break;
                        }
                        break;
                    case MOVE:
                        std::cout << "Got MOVE event:  " << event->get_name() << std::endl;
                        break;
                    default:
                        std::cout << "Got unknown event:  " << event->get_name() << std::endl;
                        break;
                }
            }
        }
};

}  // end namespace tbdmud

#endif
