#include <iostream>
#include <source_location>

namespace ret26 {
    // debugger helper
    inline void cur_line(
        const std::string_view message = "",
        const std::source_location& location = std::source_location::current())
    {
        // << location.function_name() << ":"
        if (message.size()) {
            std::cout << "[CL:" << location.file_name() << ":"   << location.line() << "=>(" << message << ")]\n"; 
        } else {
            std::cout << "[CL:" << location.file_name() << ":"   << location.line() << "]\n"; 
        }
    }

}