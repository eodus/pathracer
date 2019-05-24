//***************************************************************************
//* Copyright (c) 2015 Saint Petersburg State University
//* Copyright (c) 2011-2014 Saint Petersburg Academic University
//* All Rights Reserved
//* See file LICENSE for details.
//***************************************************************************

#pragma once


#include "utils/verify.hpp"
#include <memory>
#include <map>
#include <string>
#include <set>
#include <vector>
#include <ostream>

namespace utils {

template<class Container>
std::string ContainerToString(const Container &c) {
    std::ostringstream ss;
    ss << "Size " << c.size() << ": [";
    for (const auto &el : c)
        ss << el << ", ";
    ss << "]";
    return ss.str();
}

template<typename T>
inline const std::pair<T, T> ReversePair(std::pair<T, T> ep) {
    return std::pair<T, T>(ep.second, ep.first);
}

template<class ContainerT1, class ContainerT2>
void push_back_all(ContainerT1 &target, const ContainerT2 &to_insert) {
    target.insert(target.end(), to_insert.begin(), to_insert.end());
}

template<class ContainerT1, class ContainerT2>
void insert_all(ContainerT1 &target, const ContainerT2 &to_insert) {
    target.insert(to_insert.begin(), to_insert.end());
}

template<class MapT>
std::set<typename MapT::key_type> key_set(const MapT &m) {
    std::set<typename MapT::key_type> answer;
    for (auto it = m.begin(); it != m.end(); ++it) {
        answer.insert(it->first);
    }
    return answer;
}

template<class MapT>
std::set<typename MapT::mapped_type> value_set(const MapT &m) {
    std::set<typename MapT::mapped_type> answer;
    for (auto it = m.begin(); it != m.end(); ++it) {
        answer.insert(it->second);
    }
    return answer;
}

template<class MapT>
const typename MapT::mapped_type &get(const MapT &from, const typename MapT::key_type &key) {
    auto it = from.find(key);
    VERIFY(it != from.end());
    return it->second;
}

template<class MapT>
typename MapT::mapped_type &get(MapT &from, const typename MapT::key_type &key) {
    auto it = from.find(key);
    VERIFY(it != from.end());
    return it->second;
}

template<class MMapT>
const std::vector<typename MMapT::mapped_type> get_all(const MMapT &from, const typename MMapT::key_type &key) {
    std::vector<typename MMapT::mapped_type> answer;
    for (auto it = from.lower_bound(key); it != from.upper_bound(key); ++it) {
        answer.push_back(it->second);
    }
    return answer;
}

template<class Container, class F>
std::string join(const Container &c,
                 const std::string &delim = ", ",
                 F str_f = [] (typename Container::value_type t) { return std::to_string(t); }) {
    std::stringstream ss;
    std::string d = "";
    for (const auto &item : c) {
        ss << d << str_f(item);
        d = delim;
    }
    return ss.str();
}

}

namespace std {
template<class T1, class T2>
std::ostream &operator<<(std::ostream &os, std::pair<T1, T2> const &pair) {
    return os << "(" << pair.first << ", " << pair.second << ")";
}

template<class T>
std::ostream &operator<<(std::ostream &os, const std::vector<T> &v) {
    os << "[";
    std::string delim = "";
    for (auto it = v.begin(); it != v.end(); ++it) {
        os << delim << *it;
        delim = ", ";
    }
//     std::copy(v.begin(), v.end(), std::ostream_iterator<T>(os, ", "));
    os << "]";
    return os;
}

template<class T>
std::ostream &operator<<(std::ostream &os, const std::set<T> &set) {
    os << "{";
    bool delim = false;
    for (const auto &i : set) {
        if (delim) os << ", ";
        os << i;
        delim = true;
    }
    os << "}";
    return os;
}

template<typename K, typename V>
std::ostream &operator<<(std::ostream &os, const std::map<K, V> &map) {
    os << "{";
    bool delim = false;
    for (const auto &i : map) {
        if (delim) os << ", ";
        os << i.first << ": " << i.second;
        delim = true;
    }
    os << "}";
    return os;
}

}

