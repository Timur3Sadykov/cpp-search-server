#include <iostream>
#include <vector>
#include <set>
#include <map>

using namespace std::string_literals;

template <typename Key, typename Value>
std::ostream& operator<<(std::ostream& out, const std::pair<Key, Value>& container) {
    out << container.first << ": "s << container.second;
    return out;
}

template <typename Container>
void Print(std::ostream& out, const Container& container) {
    bool first = true;
    for (const auto& element : container) {
        if (!first) {
            out << ", "s;
        }
        out << element;
        first = false;
    }
}

template <typename Element>
std::ostream& operator<<(std::ostream& out, const std::vector<Element>& container) {
    out << '[';
    Print(out, container);
    out << ']';
    return out;
}

template <typename Element>
std::ostream& operator<<(std::ostream& out, const std::set<Element>& container) {
    out << '{';
    Print(out, container);
    out << '}';
    return out;
}

template <typename Key, typename Value>
std::ostream& operator<<(std::ostream& out, const std::map<Key, Value>& container) {
    out << '{';
    Print(out, container);
    out << '}';
    return out;
}
