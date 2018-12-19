#ifndef PITAYA_H
#define PITAYA_H

#include "pitaya/connection.h"

namespace pitaya {

class Client
{
public:
    Client();

    void Connect(const std::string& address);
    void AddEventListener(connection::Listener listener);

    // We cannot copy a Pitaya object.
    Client& operator=(const Client&) = delete;
    Client(const Client&) = delete;

private:
    connection::Connection _connection;
};

} // namespace pitaya

#endif // PITAYA_H
