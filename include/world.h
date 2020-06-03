#ifndef TBDMUD_WORLD_H_INCLUDED
#define TBDMUD_WORLD_H_INCLUDED

namespace tbdmud {

// The commands that are valid for a player to use
enum valid_commands {
    HELP,             // Help message
    WHO,              // See who is currently logged into the server
    LOOK,             // Look at the current room and what is in it
    TELL,             // Send a message to a specific player
    SAY,              // Say something to everyone in the current room
    SHOUT,            // Shout to everyone in the zone
    BROADCAST         // Broadcast a message to everyone in the world
};
    
// There is only one world object per server
// The world is the root/container for all the zones, and handles global events
class world {
    private:
        std::map<std::string, session*>               char_to_client_map;  // To refer messages to characters back to the associated session
        uint64_t                                      current_tick = 0;    // Master clock for the world (in ticks)    
        std::map<std::string, std::shared_ptr<zone>>  zones;
        std::shared_ptr<zone>                         start_zone;          // The default zone that new players should start in
        std::shared_ptr<event_queue>                  eq;

    public:
        // World Constructor
        // In the beginning....
        world() {
            std::cout << "World Created" << std::endl;
            // Create the event queue with a pointer to the world tick counter
            eq = std::shared_ptr<event_queue>(new event_queue(&current_tick));
            eq->name = "TBDWorld";

            // TODO:  Hard-coded test data until we can read it in from a file
            zones.insert({"The Zone", std::shared_ptr<zone>(new zone("The Zone", eq))});
            start_zone = zones["The Zone"];
        };

        // World Destructor (Vogons?)
        ~world() {}

        // This function should be triggered asynchronously by the server, approximately every second
        // (We're not going to synchronize to real world time)
        void tick() {
            std::cout << "tick " << current_tick << std::endl;
            current_tick++;
            //event_queue.on_tick();

            // Call on_tick() for all the zones in this world
            std::map<std::string, std::shared_ptr<zone>>::iterator z = zones.begin();

            while (z != zones.end()) {
                z->second->on_tick();
                z++;
            }
        };

        std::shared_ptr<zone> find_zone(std::string z) {
            return zones[z];
        };

        std::shared_ptr<room> find_room(std::string z, std::string r) {
            return zones[z]->get_room(r);
        };

        // Create a new character and put them in the starting room
        std::shared_ptr<character> create_character(session* client, std::string name) {
            std::cout << "world:  creating new character " << std::endl;
            std::shared_ptr<character> c = std::shared_ptr<character>(new character(name));

            char_to_client_map.insert({name, client});
            start_zone->enter_zone(c);
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
            
        // Decode commands given by the client, create events and put them in the queue
        // Multiple commands can be given on a line, separated by ;
        // Individual command arguments are separated by spaces: <command> <arg1> <arg2>, etc
        // After all explicit commands are checked for, we will check to see if the command matches valid exits from the room
        void command_parse(std::shared_ptr<session> client, std::string line)
        {
            const std::string command_delimiters = ";";
            const std::string parameter_delimiters = " ";
            std::vector<std::string> commands;
            std::shared_ptr<tbdmud::character>   pc = client->get_player()->get_character();
            std::shared_ptr<tbdmud::event_queue> eq = pc->get_event_queue();
    
            line.pop_back();  // Remove the CR that comes with the line
            boost::split(commands, line, boost::is_any_of(command_delimiters), boost::token_compress_on);  // Split the commands
    
            // Iterate over each command
            for(std::string c : commands) {
                std::vector<std::string>           parameters;
                std::vector<std::string>::iterator i;
    
                std::cout << "Parsing command:  " << c << std::endl;
    
                boost::split(parameters, c, boost::is_any_of(parameter_delimiters), boost::token_compress_on);  // Split the parameters
    
                if (boost::iequals(parameters.front(), "?")) {
                    std::cout << "Help Requested" << std::endl;
                    client->post("Valid Commands:");
                }
                else if (boost::iequals(parameters.front(), "who")) {
                    std::map<std::string, session*>::iterator c = char_to_client_map.begin();
                    std::string char_name;

                    client->post("Connected characters:");
                    while (c != char_to_client_map.end()) {
                        client->post(c->first);
                        c++;
                    }
                }
                else if (boost::iequals(parameters.front(), "look")) {
                    std::shared_ptr<room> current_room = find_room(pc->get_current_zone(), pc->get_current_room());
                    client->post(current_room->get_name());
                    
                    client->post("exits:  " + current_room->get_valid_exits());
                }
                else if (boost::iequals(parameters.front(), "tell")) {
                    std::shared_ptr<tbdmud::event_item> tell_event = std::shared_ptr<tbdmud::event_item>(new tbdmud::event_item());
                    tell_event->set_origin(client->get_player()->get_character()->get_name());
                    tell_event->set_name("TELL");
                    tell_event->set_type(tbdmud::event_type::SPEAK);
                    tell_event->set_scope(tbdmud::event_scope::TARGET);
    
                    //TODO:  Check if the target player is connected
                    std::shared_ptr<session> target;
                    bool match = false;
                    for(auto c : char_to_client_map)
                    {
                        if(c.second->get_player()->get_character()->get_name() == parameters[1]) {
                            tell_event->set_target(parameters[1]);
                            match = true;
                        }
                    }
    
                    if (match == false) {
                        std::string error = "Player " + parameters[1] + " is not connected.\n"; 
                        client->post(error);
                        return;
                    }
    
                    // Chop the first and second words off the string, keep the rest as the message
                    for (int j = 0; j < 2; j++) {
                        size_t space = c.find(" ");    
                        if (space != std::string::npos) {
                          c = c.substr(space + 1);
                        }
                    }
    
                    tell_event->set_message(tbdmud::event_scope::TARGET, c);
    
                    std::cout << "tell event to :  " << tell_event->get_target() << " : " << tell_event->get_message(tbdmud::event_scope::TARGET) << std::endl;
                    eq->add_event(tell_event);
                }
                else if (boost::iequals(parameters.front(), "say")) {
                    std::shared_ptr<tbdmud::event_item> say_event = std::shared_ptr<tbdmud::event_item>(new tbdmud::event_item());
                    say_event->set_origin(client->get_player()->get_character()->get_name());
                    say_event->set_name("SAY");
                    say_event->set_type(tbdmud::event_type::SPEAK);
                    say_event->set_scope(tbdmud::event_scope::ROOM);
    
                    // Chop the first word off the string, keep the rest as the message
                    size_t space = c.find(" ");    
                    if (space != std::string::npos) {
                      c = c.substr(space + 1);
                    }
                    say_event->set_message(tbdmud::event_scope::ROOM, c);
    
                    std::cout << "say event:  " << say_event->get_name() << std::endl;
                    eq->add_event(say_event);
                }
                // TODO:  Remove temporary command "say with delay" for testing event delays
                else if (boost::iequals(parameters.front(), "dsay")) {
                    std::shared_ptr<tbdmud::event_item> dsay_event = std::shared_ptr<tbdmud::event_item>(new tbdmud::event_item());
                    dsay_event->set_origin(client->get_player()->get_character()->get_name());
                    dsay_event->set_name("DSAY");
                    dsay_event->set_rtick(5);
                    dsay_event->set_type(tbdmud::event_type::SPEAK);
                    dsay_event->set_scope(tbdmud::event_scope::ROOM);
    
                    // Chop the first word off the string, keep the rest as the message
                    size_t space = c.find(" ");    
                    if (space != std::string::npos) {
                      c = c.substr(space + 1);
                    }
                    dsay_event->set_message(tbdmud::event_scope::ROOM, c);
    
                    std::cout << "dsay event:  " << dsay_event->get_name() << std::endl;
                    eq->add_event(dsay_event);
                }
                else if (boost::iequals(parameters.front(), "shout")) {
                    std::shared_ptr<tbdmud::event_item> shout_event = std::shared_ptr<tbdmud::event_item>(new tbdmud::event_item());
                    shout_event->set_origin(client->get_player()->get_character()->get_name());
                    shout_event->set_name("SHOUT");
                    shout_event->set_type(tbdmud::event_type::SPEAK);
                    shout_event->set_scope(tbdmud::event_scope::ZONE);
    
                    // Chop the first word off the string, keep the rest as the message
                    size_t space = c.find(" ");    
                    if (space != std::string::npos) {
                      c = c.substr(space + 1);
                    }
                    shout_event->set_message(tbdmud::event_scope::ZONE, c);
    
                    std::cout << "shout event:  " << shout_event->get_name() << std::endl;
                    eq->add_event(shout_event);
                }
                else if (boost::iequals(parameters.front(), "broadcast")) {
                    std::shared_ptr<tbdmud::event_item> broadcast_event = std::shared_ptr<tbdmud::event_item>(new tbdmud::event_item());
                    broadcast_event->set_origin(client->get_player()->get_character()->get_name());
                    broadcast_event->set_name("BROADCAST");
                    broadcast_event->set_type(tbdmud::event_type::SPEAK);
                    broadcast_event->set_scope(tbdmud::event_scope::WORLD);
    
                    // Chop the first word off the string, keep the rest as the message
                    size_t space = c.find(" ");    
                    if (space != std::string::npos) {
                      c = c.substr(space + 1);
                    }
                    broadcast_event->set_message(tbdmud::event_scope::ZONE, c);
    
                    std::cout << "broadcast event:  " << broadcast_event->get_name() << std::endl;
                    eq->add_event(broadcast_event);
                }
                else {
                    // TODO:  See if parameters.front() equals one of the defined exits from the room
                }
            }
        }
};

}  // end namespace tbdmud

#endif
