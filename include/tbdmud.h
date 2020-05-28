// This class contains information and methods relating to the current player, and is created when a new telnet session is started
// There is exactly one player per telnet connection
// The player object is populated with information from the user file and the system on creation
// The player's name is unique for this server.
#ifndef TBDMUD_H_INCLUDED
#define TBDMUD_H_INCLUDED

#include <optional>

namespace tbdmud {

// To hold data about the character currently being used by a player
class character {
    private:
    public:
        //std::optional<tbdmud::player> player;  // The player currently playing this character (it is possible we can have characters driven by NPC, without a player)
        std::string                   name;

        // Default Constructor
        //character() {};
        //character(int session) {
        //    session_id = session;
        //};
};

// To hold data regarding the currently connected player
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
        //player() {};
        //player(int session) {
        //    session_id = session;
        //};
};

}  // end namespace tbdmud

#endif
