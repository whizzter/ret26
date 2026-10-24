#pragma once
#include <string>
#include <string_view>
#include <meta>

namespace ret26 {
    // stringly static storage string type written to help with reflection metaprogramming.
    struct static_string {
        const char * m_start;
        const size_t m_size;

        constexpr const char* data() const {
            return m_start;
        }
        constexpr char operator[](size_t idx) const {
            return m_start[idx];
        }
        constexpr size_t size() const {
            return m_size;
        }
        
        constexpr std::string_view as_view() const {
            return {m_start,size()};
        }

        consteval static_string substr(size_t start,size_t count) const {
            return static_string{as_view().subview(start,count)};
        }

        template<size_t N>
        consteval static_string(const char (&s)[N]) 
            : m_start{std::define_static_string(std::string_view{s,N-1})},m_size{N-1} {}

        // constexpr static_string(const char * start,size_t size) : m_start(start),m_size(size) {}
        // consteval static_string(const char *start, size_t sz)
        //     : m_start(std::define_static_string(start)),m_size(sz){}
        consteval static_string(const std::string_view init)
            : m_start(std::define_static_string(init)),m_size(init.size()){}
        
        // consteval static_string(const static_string& other) = default;
        // consteval static_string& operator=(const static_string& other) = default;
        // consteval static_string(static_string&& other) = default;
        // consteval static_string& operator=(static_string&& other) = default;
        // consteval ~static_string() = default;
    };

    consteval static_string operator+(const char * first,const static_string &second) {
        std::string tmp;
        tmp.append(first);
        tmp.append(second.data(),second.size());
        return static_string{std::define_static_string(tmp)};
    }

}