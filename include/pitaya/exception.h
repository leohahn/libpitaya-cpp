#ifndef PITAYA_EXCEPTION_H
#define PITAYA_EXCEPTION_H

#include <boost/system/error_code.hpp>
#include <stdexcept>
#include <string>

namespace pitaya {

class Exception : public std::exception
{
public:
    Exception(const std::string& what)
        : _what(what)
    {}

    const char* what() const throw() { return _what.c_str(); }

private:
    std::string _what;
};

class ErrorCategory : public boost::system::error_category
{
public:
    const char* name() const noexcept override { return "pitaya"; }

    std::string message(int ev) const override { return "Returning error category message man"; }
};

} // namespace pitaya

#endif // PITAYA_EXCEPTION_H