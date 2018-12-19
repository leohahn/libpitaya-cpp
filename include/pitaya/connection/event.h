#ifndef PITAYA_CONNECTION_EVENT_H
#define PITAYA_CONNECTION_EVENT_H

#include <functional>
#include <mutex>
#include <ostream>
#include <string>
#include <vector>

namespace pitaya {
namespace connection {

enum class Event
{
    Connected,
    ConnectionFailed,
};

inline std::ostream&
operator<<(std::ostream& os, Event ev)
{
    switch (ev) {
        case Event::Connected:
            os << "Connected";
            break;
        case Event::ConnectionFailed:
            os << "Connection Failed";
            break;
    }
    return os;
}

using Listener = std::function<void(Event, const std::string&)>;

class EventListeners
{
public:
    void lock() { _mutex.lock(); }
    void unlock() { _mutex.unlock(); }

    void Add(Listener listener) { _listeners.push_back(std::move(listener)); }

    void Broadcast(Event ev, const std::string& msg)
    {
        for (const auto& l : _listeners) {
            l(ev, msg);
        }
    }

    std::vector<Listener>& Get() { return _listeners; }
    const std::vector<Listener>& Get() const { return _listeners; }

private:
    std::mutex _mutex;
    std::vector<Listener> _listeners;
};

} // namespace connection
} // namespace pitaya

#endif // PITAYA_CONNECTION_EVENT_H