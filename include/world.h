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

        // World States
        bool state_sun = false;   // Is the sun up?
        bool state_moon = false;  // Is the moon up?

    public:
        // World Constructor
        // In the beginning....
        world() {
            std::cout << "World Created" << std::endl;
            // Create the event queue with a pointer to the world tick counter
            eq = std::shared_ptr<event_queue>(new event_queue(&current_tick));
            eq->name = "TBDWorld";

            // TODO:  Hard-coded test data until we can read it in from a file
            zones.insert({"Zion", std::shared_ptr<zone>(new zone("Zion", eq))});
            start_zone = zones["Zion"];
        };

        // World Destructor (Here there be Vogons)
        ~world() {}

        // This function should be triggered asynchronously by the server, approximately every second
        // (We're not synchronizing to real world time)
        void tick() {
            if (current_tick % 100 == 0) std::cout << "tick " << current_tick << std::endl;
            current_tick++;

            // Call on_tick() for all the zones in this world, who will call it on all the rooms, who will call it on all the characters/objects
            std::map<std::string, std::shared_ptr<zone>>::iterator z = zones.begin();
            while (z != zones.end()) {
                z->second->on_tick();
                z++;
            }

            periodic_events(current_tick);   // After processing the tick see if there are periodic world events to handle/create
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

            // Broadcast to everyone else that a new player entered the room
            for (std::shared_ptr<character> ch : start_zone->get_start_room()->get_characters()) {
                char_to_client_map[ch->get_name()]->post("\n" + name + " has entered the room.\n");
            }

            char_to_client_map.insert({name, client});
            start_zone->enter_zone(c);
            start_zone->get_start_room()->enter_room(c);

            client->post("\nYou have entered the room.\n");

            client->post("\nYou are in:  " + start_zone->get_start_room()->get_name() + "\n");
            client->post("exits:  " + start_zone->get_start_room()->get_exits_str() + "\n");
            client->post("\nStanding around:\n" + start_zone->get_start_room()->get_character_str() + "\n");

            return c;
        };

        // Register an existing character and put them in the starting room
        // TODO:  Put them in the room they logged out in
        void register_character(session* client, std::shared_ptr<character> c) {
            std::cout << "world:  registering character " << c->get_name() << std::endl;

            char_to_client_map.insert({c->get_name(), client});
            start_zone->enter_zone(c);
            start_zone->get_start_room()->enter_room(c);
        };

        // Delete a character - remove them from the room they are in and other cleanup
        void remove_character(std::string character_name) {
            std::cout << "world:  removing character " << character_name << std::endl;

            char_to_client_map.erase(character_name);
            // TODO:  Remove from zone
            // TODO:  Remove from room
        };

        /***********************************************************************************************
         * PERIODIC EVENTS
         * Create periodic events that happen in the world
         ***********************************************************************************************/  
        void periodic_events(int current_tick) {

            // Periodically make the sun rise or set
            if (current_tick % 42 == 0) {
                std::shared_ptr<tbdmud::event_item> sun_event = std::shared_ptr<tbdmud::event_item>(new tbdmud::event_item());
                sun_event->set_origin("world");  // TODO:  pick something that doesn't mess up if a character names themselves that
                sun_event->set_name("SUN");
                sun_event->set_type(tbdmud::event_type::NOTICE);
                sun_event->set_scope(tbdmud::event_scope::WORLD);
                if (!state_sun) {
                    sun_event->set_message(tbdmud::event_scope::WORLD, "The sun rises");
                    state_sun = true;
                }
                else {
                    sun_event->set_message(tbdmud::event_scope::WORLD, "The sun sets");
                    state_sun = false;
                }
                eq->add_event(sun_event);
            }  

            // Periodically make the moon rise or set
            if (current_tick % 67 == 0) {
                std::shared_ptr<tbdmud::event_item> moon_event = std::shared_ptr<tbdmud::event_item>(new tbdmud::event_item());
                moon_event->set_origin("world");  // TODO:  pick something that doesn't mess up if a character names themselves that
                moon_event->set_name("SUN");
                moon_event->set_type(tbdmud::event_type::NOTICE);
                moon_event->set_scope(tbdmud::event_scope::WORLD);
                if (!state_moon) {
                    if (state_sun) {
                        moon_event->set_message(tbdmud::event_scope::WORLD, "You can barely see the moon rising");
                    }
                    else {
                        moon_event->set_message(tbdmud::event_scope::WORLD, "The moon rises");
                    }
                    state_moon = true;
                }
                else {
                    moon_event->set_message(tbdmud::event_scope::WORLD, "The moon sets");
                    state_moon = false;
                }
                eq->add_event(moon_event);
            }  
        }

        /***********************************************************************************************
         * COMMAND PARSER
         * Decode commands given by the client, create events and put them in the priority queue
         * Multiple commands can be given on a line, separated by ;
         * Individual command arguments are separated by spaces: <command> <arg1> <arg2>, etc
         * After all explicit commands are checked for, we will check to see if the command matches valid exits from the room
         ***********************************************************************************************/  
        void command_parse(std::shared_ptr<session> client, std::string line)
        {
            const std::string command_delimiters = ";";
            const std::string parameter_delimiters = " ";
            std::vector<std::string> commands;
            std::shared_ptr<tbdmud::character>   pc = client->get_player()->get_character();
            std::shared_ptr<tbdmud::event_queue> eq = pc->get_event_queue();
    
            line.pop_back();  // Remove the LF that comes with the line
            line.pop_back();  // Remove the CR that comes with the line
            boost::split(commands, line, boost::is_any_of(command_delimiters), boost::token_compress_on);  // Split the commands
    
            // Iterate over each command
            for(std::string c : commands) {
                #ifdef DEBUG
                std::cout << "Parsing command line:  " << c << std::endl;
                #endif

                std::vector<std::string> v_command;
                std::size_t c_position;
                std::string message;

                message = c;  // It's a little wasteful of memory, but we're going to keep a copy of the message instead of reconstructing it from the vector

                // Chop the first word off the remaining message, keep the rest as the message to send
                size_t space = message.find(" ");    
                if (space != std::string::npos) {
                  message = message.substr(space + 1);
                }

                // Iterate over the original command string and parse into a vector - words separated by spaces
                while((c_position = c.find(' ')) != std::string::npos )
                {   
                    // If there's multiple words in the command then this gets called first
                    v_command.push_back(c.substr(0, c_position));
                    c = c.substr(c_position + 1);
                }
                v_command.push_back(c);

                #ifdef DEBUG
                std::cout << "Processing command:  " << v_command[0] << std::endl;
                #endif

                /***** ?/HELP *****/
                if ((v_command[0].at(0) == '?') || (boost::iequals(v_command[0], "help"))) {
                    client->post("\nHelp - Valid Commands:\n");
                    client->post("? or HELP       : help\n");
                    client->post("who             : show connected players\n");
                    client->post("look/l          : show room description\n");
                    client->post("tell player ... : only player hears ...\n");
                    client->post("say ...         : everyone in the room hears ...\n");
                    client->post("shout ...       : everyone in the zone hears ...\n");
                    client->post("broadcast ...   : everyone connected hears ...\n\n");
                }
                /***** who *****/
                else if (boost::iequals(v_command[0], "who")) {
                    client->post("\nConnected:\n");

                    std::map<std::string, session*>::iterator c = char_to_client_map.begin();
                    while (c != char_to_client_map.end()) {
                        client->post(c->first + "\n");
                        c++;
                    }
                    client->post("\n");
                }
                /***** look/l *****/
                else if ((boost::iequals(v_command[0], "look")) || ((v_command.size() == 1) && boost::iequals(v_command[0], "l"))) {
                    #ifdef DEBUG
                    std::cout << "parsing look" << std::endl;
                    std::cout << "current_room = " << pc->get_current_room() << std::endl;
                    std::cout << "current_zone = " << pc->get_current_zone() << std::endl;
                    #endif
                    std::shared_ptr<room> current_room = find_room(pc->get_current_zone(), pc->get_current_room());

                    client->post("\nYou are in:  " + current_room->get_name() + "\n");
                    client->post("exits:  " + current_room->get_exits_str() + "\n");
                    client->post("\nStanding around:\n" + current_room->get_character_str() + "\n");
                }
                /***** tell <player> ... *****/
                else if (boost::iequals(v_command[0], "tell")) {
                    if(v_command.size() < 3) {
                        client->post("Bad tell command format, expected:  tell player ...\n");
                        break;
                    }

                    std::shared_ptr<tbdmud::event_item> tell_event = std::shared_ptr<tbdmud::event_item>(new tbdmud::event_item());
                    tell_event->set_origin(pc->get_name());
                    tell_event->set_name("TELL");
                    tell_event->set_type(tbdmud::event_type::SPEAK);
                    tell_event->set_scope(tbdmud::event_scope::TARGET);

                    // Check if the target player is connected
                    std::shared_ptr<session> target;
                    bool match = false;
                    for(auto c : char_to_client_map)
                    {
                        if(c.second->get_player()->get_character()->get_name() == v_command[1]) {
                            tell_event->set_target(v_command[1]);
                            match = true;
                        }
                    }

                    if (match == false) {
                        std::string error = "Player " + v_command[1] + " is not connected.\n"; 
                        client->post(error);
                        return;
                    }

                    // Chop the first word (the target name) off the remaining message, send the rest
                    size_t space = message.find(" ");    
                    if (space != std::string::npos) {
                      message = message.substr(space + 1);
                    }

                    tell_event->set_message(tbdmud::event_scope::TARGET, message);
                    std::cout << "tell event from " << tell_event->get_origin() << " to " << tell_event->get_target() << " : " << tell_event->get_message(tbdmud::event_scope::TARGET) << std::endl;
                    eq->add_event(tell_event);
                }
                /***** say ... *****/
                else if (boost::iequals(v_command[0], "say")) {
                    if(v_command.size() < 2) {
                        client->post("Bad say command format, expected:  say ...\n");
                        break;
                    }

                    std::shared_ptr<tbdmud::event_item> say_event = std::shared_ptr<tbdmud::event_item>(new tbdmud::event_item());

                    say_event->set_origin(client->get_player()->get_character()->get_name());
                    say_event->set_name("SAY");
                    say_event->set_type(tbdmud::event_type::SPEAK);
                    say_event->set_scope(tbdmud::event_scope::ROOM);
                    say_event->set_message(tbdmud::event_scope::ROOM, message);

                    std::cout << "say event:  " << say_event->get_name() << ": " << say_event->get_message(tbdmud::event_scope::ROOM) << std::endl;
                    eq->add_event(say_event);
                }
                /***** dsay ... *****/
                // TODO:  Remove temporary command "say with delay" for testing event delays
                else if (boost::iequals(v_command[0], "dsay")) {
                    if(v_command.size() < 3) {
                        client->post("Bad dsay command format, expected:  say # ...\n");
                        break;
                    }

                    // Chop the first word (the delay time) off the remaining message, send the rest
                    size_t space = message.find(" ");    
                    if (space != std::string::npos) {
                      message = message.substr(space + 1);
                    }

                    std::shared_ptr<tbdmud::event_item> dsay_event = std::shared_ptr<tbdmud::event_item>(new tbdmud::event_item());

                    dsay_event->set_origin(client->get_player()->get_character()->get_name());
                    dsay_event->set_name("DSAY");
                    dsay_event->set_rtick(std::stoi(v_command[1]));
                    dsay_event->set_type(tbdmud::event_type::SPEAK);
                    dsay_event->set_scope(tbdmud::event_scope::ROOM);
                    dsay_event->set_message(tbdmud::event_scope::ROOM, message);

                    std::cout << "dsay event:  " << dsay_event->get_name() << " - " << dsay_event->get_rtick() << ":  " << dsay_event->get_message(tbdmud::event_scope::ROOM) << std::endl;
                    eq->add_event(dsay_event);
                }
                /***** shout ... *****/
                else if (boost::iequals(v_command[0], "shout")) {
                    if(v_command.size() < 2) {
                        client->post("Bad shout command format, expected:  shout # ...\n");
                        break;
                    }

                    std::shared_ptr<tbdmud::event_item> shout_event = std::shared_ptr<tbdmud::event_item>(new tbdmud::event_item());

                    shout_event->set_origin(client->get_player()->get_character()->get_name());
                    shout_event->set_name("SHOUT");
                    shout_event->set_type(tbdmud::event_type::SPEAK);
                    shout_event->set_scope(tbdmud::event_scope::ZONE);
                    shout_event->set_message(tbdmud::event_scope::ZONE, message);

                    std::cout << "shout event:  " << shout_event->get_name() << ":  " << shout_event->get_message(tbdmud::event_scope::ZONE) << std::endl;
                    eq->add_event(shout_event);
                }
                /***** broadcast ... *****/
                else if (boost::iequals(v_command[0], "broadcast")) {
                    if(v_command.size() < 2) {
                        client->post("Bad broadcast command format, expected:  broadcast # ...\n");
                        break;
                    }
                    std::shared_ptr<tbdmud::event_item> broadcast_event = std::shared_ptr<tbdmud::event_item>(new tbdmud::event_item());

                    broadcast_event->set_origin(client->get_player()->get_character()->get_name());
                    broadcast_event->set_name("BROADCAST");
                    broadcast_event->set_type(tbdmud::event_type::SPEAK);
                    broadcast_event->set_scope(tbdmud::event_scope::WORLD);
                    broadcast_event->set_message(tbdmud::event_scope::WORLD, message);

                    std::cout << "broadcast event:  " << broadcast_event->get_name() << ":  " << broadcast_event->get_message(tbdmud::event_scope::WORLD) << std::endl;
                    eq->add_event(broadcast_event);
                }
                else {
                    bool matches_exit = false;

                    /***** move *****/
                    // If the command is only one word, look to see if it matches one of the exits from the current room
                    if(v_command.size() == 1) {
                        std::shared_ptr<room> origin_room = find_room(pc->get_current_zone(), pc->get_current_room());
                        std::map<std::string, std::shared_ptr<room>> exits = origin_room->get_exits();
                        std::map<std::string, std::shared_ptr<room>>::iterator e = exits.begin();

                        while (e != exits.end()) {
                            #ifdef DEBUG
                            std::cout << "move:  exits = " << e->first << ", " << e->second->get_name() << std::endl;
                            #endif

                            // If the first (and only) word of the command equals one of the exits from the current room, create a move event to that room
                            if (boost::iequals(v_command[0],  e->first)) {
                                matches_exit = true;
                                std::shared_ptr<tbdmud::event_item> move_event = std::shared_ptr<tbdmud::event_item>(new tbdmud::event_item());

                                move_event->set_origin(client->get_player()->get_character()->get_name());
                                move_event->set_origin_room(origin_room->get_name());
                                move_event->set_target_room(e->second->get_name());
                                move_event->set_name("MOVE");
                                move_event->set_type(tbdmud::event_type::MOVE);
                                move_event->set_scope(tbdmud::event_scope::ROOM);

                                #ifdef DEBUG
                                std::cout << "move event:  move " << move_event->get_origin() << " from " << move_event->get_origin_room() << " to " << move_event->get_target_room() << std::endl;
                                #endif
                                eq->add_event(move_event);
                            }
                            e++;
                        }

                    }

                    if (!matches_exit) {
                        #ifdef DEBUG
                        std::cout << "\nUnknown command or exit\n" << std::endl;
                        #endif
                        client->post("\nUnknown command or exit\n");
                    }
                }
            }  // end for (commands)
        }; // end command_parse()

        /***********************************************************************************************
         * EVENT PROCESSOR
         * Take an event and decode the event type, scope, and other parameters to determine the actions
         * the world should take in response (sending a message to a client's screen, moving a character
         * from one room to another, etc)
         ***********************************************************************************************/  
        void process_events() {
            // Grab the event at the top of the event queue
            std::shared_ptr<event_item>        event = eq->next_event();
            if (event == nullptr) return;  // The queue will return nullptr if there are no events that need processing

            // We have to define all these here because we can't do it inside the case statment
            std::string                origin_name;
            session*                   origin_client    = nullptr;
            std::shared_ptr<character> origin_char      = nullptr;
            std::shared_ptr<zone>      origin_zone      = nullptr;
            std::string                origin_room_name;
            std::shared_ptr<room>      origin_room      = nullptr; 
            std::string                target_name;
            session*                   target_client    = nullptr;
            std::shared_ptr<character> target_char      = nullptr;
            std::shared_ptr<zone>      target_zone      = nullptr; 
            std::string                target_room_name;
            std::shared_ptr<room>      target_room      = nullptr; 
            std::string                message;
            std::map<std::string, session*>::iterator  ch;

            if (event == nullptr) {
                std::cout << "Error - NULL event" << std::endl; 
            }
            else {
                switch(event->get_type()) {
                    case NOTICE:
                        // Get the relevant fields for NOTICE events
                        origin_name      = event->get_origin();

                        message = event->get_message(event_scope::WORLD);
                        std::cout << "NOTICE event:  " << message << std::endl;

                        // Broadcast to everyone in the world - these messages don't have an origin or specific target
                        ch = char_to_client_map.begin();
                        while (ch != char_to_client_map.end()) {
                            std::string target_name = ch->first;
                            ch->second->post("\n" + message + "\n\n");
                            ch++;
                        }

                        break;
                    case SPEAK:
                        // Get the relevant fields for SPEAK events
                        origin_name      = event->get_origin();
                        origin_room_name = event->get_origin_room();
                        target_name      = event->get_target();
                        target_room_name = event->get_target_room();

                        // TODO:  develop a better way to get the character pointer given the character name
                        if (origin_name != "") {
                            origin_char = char_to_client_map[origin_name]->get_player()->get_character();
                            origin_client = char_to_client_map[origin_name];
                        }

                        switch(event->get_scope()) {
                            case TARGET:  // TELL Event
                                if ((origin_name != "") && (target_name != "")) {
                                    std::cout << "TELL to " << target_name << ":  " << event->get_message(TARGET) << std::endl;
                                    target_char = char_to_client_map[target_name]->get_player()->get_character();
                                    target_client = char_to_client_map[target_name];

                                    // Write the messages out to the origin and target clients
                                    if (target_client != nullptr) {
                                        target_client->post("\n" + origin_name + " tells you: " + event->get_message(TARGET) + "\n\n");
                                    }
                                    if (origin_client != nullptr) {
                                        origin_client->post("\nYou tell " + target_name + ":  " + event->get_message(TARGET) + "\n\n");
                                    }
                                }
                                else {
                                    std::cout << "Error - Malformed TELL event\n" << std::endl;
                                }

                                break;
                            case ROOM:  // SAY Event
                                message = event->get_message(event_scope::ROOM);
                                std::cout << "SAY event:  " << message << std::endl;

                                origin_room = find_room(origin_char->get_current_zone(), origin_char->get_current_room());
                                
                                // Broadcast to everyone else in the room what the origin player said
                                for (std::shared_ptr<character> ch : origin_room->get_characters()) {
                                    if (ch->get_name() != origin_name) {
                                        char_to_client_map[ch->get_name()]->post("\n" + origin_name + " says:  " + message + "\n\n");
                                    }
                                    else {
                                        origin_client->post("\nYou say:  " + message + "\n\n");
                                    }
                                }

                                break;
                            case ZONE:  // Shout Event
                                message = event->get_message(event_scope::ZONE);
                                std::cout << "SHOUT event:  " << message << std::endl;

                                origin_zone = find_zone(origin_char->get_current_zone());
                                
                                // Broadcast to everyone else in the zone what the origin player said
                                for (std::shared_ptr<character> ch : origin_zone->get_characters()) {
                                    if (ch->get_name() != origin_name) {
                                        char_to_client_map[ch->get_name()]->post("\n" + origin_name + " shouts:  " + message + "\n\n");
                                    }
                                    else {
                                        origin_client->post("\nYou shout:  " + message + "\n\n");
                                    }
                                }

                                break;
                            case WORLD:  // Broadcast Event
                                message = event->get_message(event_scope::WORLD);
                                std::cout << "BROADCAST event:  " << message << std::endl;

                                // Broadcast to everyone else in the world what the origin player said
                                ch = char_to_client_map.begin();
                                while (ch != char_to_client_map.end()) {
                                    std::string target_name = ch->first;
                                    if (target_name != origin_name) {
                                        ch->second->post("\n" + origin_name + " broadcasts:  " + message + "\n\n");
                                    }
                                    else {
                                        origin_client->post("\nYou broadcast:  " + message + "\n\n");
                                    }
                                    ch++;
                                }

                                break;
                            default:
                                std::cout << "Error - Unknown SPEAK event:  " << event->get_name() << std::endl;
                                break;
                        }
                        break;
                    case MOVE:
                        // Get the relevant fields for MOVE events
                        origin_name      = event->get_origin();
                        origin_room_name = event->get_origin_room();
                        target_room_name = event->get_target_room();

                        // TODO:  develop a better way to get the character pointer given the character name
                        if (origin_name != "") {
                            origin_char = char_to_client_map[origin_name]->get_player()->get_character();
                            origin_client = char_to_client_map[origin_name];
                        }

                        if (origin_room_name != "" && target_room_name != "") {
                            // TODO:  Handle moving from one zone to another
                            //origin_zone = find_zone(origin_char->get_current_zone());
                            origin_room = find_room(origin_char->get_current_zone(), origin_room_name);
                            target_room = find_room(origin_char->get_current_zone(), target_room_name);
                            std::cout << "MOVE event:  move " << origin_name << " from " << origin_room_name << " to " << target_room_name << std::endl;

                            // Broadcast to everyone else in the origin room that the player left
                            for (std::shared_ptr<character> ch : origin_room->get_characters()) {
                                if (ch->get_name() != origin_name) {
                                    char_to_client_map[ch->get_name()]->post("\n" + origin_name + " left the room towards " + target_room_name + "\n\n");
                                }
                                else {
                                    origin_client->post("\nYou left the room\n\n");
                                }
                            }

                            // Actually perform the room transition
                            origin_room->leave_room(origin_char);
                            target_room->enter_room(origin_char);

                            // Broadcast to everyone else in the target room that the player has arrived
                            for (std::shared_ptr<character> ch : target_room->get_characters()) {
                                if (ch->get_name() != origin_name) {
                                    char_to_client_map[ch->get_name()]->post("\n" + origin_name + " has entered the room\n\n");
                                }
                                else {
                                    origin_client->post("\nYou have entered " + target_room_name + "\n\n");
                                }
                            }
                        }
                        else {
                            std::cout << "Error - Malformed MOVE event\n" << std::endl;
                        }
                        break;
                    default:
                        std::cout << "Unknown event:  " << event->get_name() << std::endl;
                        break;
                }
            }
        };
};

}  // end namespace tbdmud

#endif
