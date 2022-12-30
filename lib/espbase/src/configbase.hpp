#pragma once

#include <algorithm>
#include <functional>
#include <map>
#include <string>

#include "serialize.hpp"

namespace espbase {

struct type_info_t {
    size_t size;
    bool is_array;
    size_t extent;
    type_info_t *element_info;
};

template<typename T>
inline type_info_t *type_info() {
    static type_info_t id = {
        .size = sizeof(T),
        .is_array = std::is_array<T>::value,
        .extent = std::extent<T>::value,
        .element_info = std::is_array<T>::value ? type_info<typename std::remove_extent<T>::type>() : &id,
    };
    return &id;
}

struct config_base {
    class meta {
    public:
        struct data {
            char *default_value = nullptr;
            type_info_t *type_info;
            std::function<void(const char *)> set;
            std::function<void(char *)> get;
            std::function<void()> on_change = []{};
        };

        inline void registerMember(const char *name, data *metadata){
            m_members[name] = metadata; }

        inline std::map<std::string, data *> const& members() const {
            return m_members; }

        const data *metadata(const char *member) const;
        void reset_all() const;
        bool reset(const char *member) const;
        size_t size(const char *member) const;
        bool set(const char *member, const char *data) const;
        bool get(const char *member, char *data) const;
        void on_change(const char *member, std::function<void()>&& callback);
        void notify_all() const;
        void notify(const char *member) const;

    private:
        std::map<std::string, data *> m_members;
    };

    template<class T>
    struct member {
        using type = T;
        T value;

        member(member const&) = delete;
        member(meta *meta, const char *name, T const& default_value = {});
        template<size_t ParamSize>
        member(meta *meta, const char *name,
               const typename std::remove_extent<T>::type (&value)[ParamSize]);
        inline size_t get(char *value) const { return serialize(this->value, value); }
        inline void set(const char *data) { deserialize(data, this->value); }
        inline T& operator= (T const& v) { this->value = v; return this->value; }
        inline operator T&() { return value; }
        inline operator const T&() const { return value; }
    };
};


template<class T>
inline config_base::member<T>::member(meta *meta, const char *name, T const& default_value) {
    static_assert(sizeof(member<T>) == sizeof(T));

    auto *d = new meta::data;
    d->set = std::bind(&member<T>::set, this, std::placeholders::_1);
    d->get = std::bind(&member<T>::get, this, std::placeholders::_1);
    d->default_value = new char[sizeof(T)];
    d->type_info = type_info<T>();
    serialize(default_value, d->default_value);

    meta->registerMember(name, d);
}

template<class T>
template<size_t N>
inline config_base::member<T>::member(
    meta *meta, const char *name,
    const typename std::remove_extent<T>::type (&value)[N])
{
    static_assert(N < std::extent<T>::value);
    T default_value = {};
    std::copy(std::begin(value), std::end(value), default_value);
    new(this) member(meta, name, default_value);
}


inline const config_base::meta::data *config_base::meta::metadata(const char *member) const {
    auto itr = m_members.find(member);
    return (itr != m_members.end()) ? itr->second : nullptr;
}

inline void config_base::meta::reset_all() const {
    for (auto itr = m_members.begin(); itr != m_members.end(); ++itr) {
        itr->second->set(itr->second->default_value);
        itr->second->on_change();
    }
}

inline bool config_base::meta::reset(const char *member) const {
    if (auto meta = metadata(member)) {
        meta->set(meta->default_value);
        meta->on_change();
        return true;
    }
    return false;
}

inline size_t config_base::meta::size(const char *member) const {
    if (auto meta = metadata(member))
        return meta->type_info->size;

    return 0;
}

inline bool config_base::meta::set(const char *member, const char *data) const {
    if (auto meta = metadata(member)) {
        meta->set(data);
        meta->on_change();
        return true;
    }
    return false;
}

inline bool config_base::meta::get(const char *member, char *data) const {
    if (auto meta = metadata(member)) {
        meta->get(data);
        return true;
    }
    return false;
}

inline void config_base::meta::on_change(const char *member, std::function<void()>&& callback) {
    auto itr = m_members.find(member);
    if (itr != m_members.end())
        itr->second->on_change = callback ? std::move(callback) : []{};
}

inline void config_base::meta::notify_all() const {
    for (auto itr = m_members.begin(); itr != m_members.end(); ++itr) {
        itr->second->on_change();
    }
}

inline void config_base::meta::notify(const char *member) const {
    if (auto meta = metadata(member))
        meta->on_change();
}

}
