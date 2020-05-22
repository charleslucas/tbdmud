// This class contains information and methods relating to the current player, and is created when a new telnet session is started
// There is exactly one player per telnet connection
// The player object is populated with information from the user file and the system on creation
// The player's name is unique for this server.
#ifndef PLAYER_H_INCLUDED
#define PLAYER_H_INCLUDED

namespace tbdmud {

class player {
    private:
        int    session_id;
    //    //std::string username;

    public:
        // Default Constructor
        player() {};
        player(int session) {
            session_id = session;
        };
};

}

#endif
