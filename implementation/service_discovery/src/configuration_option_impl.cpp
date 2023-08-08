// Copyright (C) 2014-2018 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <cstring>
#include <vsomeip/internal/logger.hpp>

#include "../include/configuration_option_impl.hpp"
#include "../../message/include/deserializer.hpp"
#include "../../message/include/serializer.hpp"

namespace vsomeip_v3 {
namespace sd {

configuration_option_impl::configuration_option_impl() {
    length_ = 2; // always contains "Reserved" and the trailing '\0'
    type_ = option_type_e::CONFIGURATION;
}

configuration_option_impl::~configuration_option_impl() {
}

bool
configuration_option_impl::operator ==(const option_impl &_other) const {
    bool is_equal(option_impl::operator ==(_other));

    if (is_equal) {
        const configuration_option_impl &its_other
            = dynamic_cast<const configuration_option_impl &>(_other);
        is_equal = (configuration_ == its_other.configuration_);
    }

    return is_equal;
}

void configuration_option_impl::add_item(const std::string &_key,
        const std::string &_value) {
    configuration_[_key] = _value;
    length_ = uint16_t(length_ + _key.length() + _value.length() + 2u); // +2 for the '=' and length
}

void configuration_option_impl::remove_item(const std::string &_key) {
    auto it = configuration_.find(_key);
    if (it != configuration_.end()) {
        length_ = uint16_t(length_ - (it->first.length() + it->second.length() + 2u));
        configuration_.erase(it);
    }
}

std::vector<std::string> configuration_option_impl::get_keys() const {
    std::vector < std::string > l_keys;
    for (const auto& elem : configuration_)
        l_keys.push_back(elem.first);
    return l_keys;
}

std::vector<std::string> configuration_option_impl::get_values() const {
    std::vector < std::string > l_values;
    for (const auto& elem : configuration_)
        l_values.push_back(elem.second);
    return l_values;
}

std::string configuration_option_impl::get_value(
        const std::string &_key) const {
    std::string l_value("");
    auto l_elem = configuration_.find(_key);
    if (l_elem != configuration_.end())
        l_value = l_elem->second;
    return l_value;
}

bool configuration_option_impl::serialize(vsomeip_v3::serializer *_to) const {
    bool is_successful;
    std::string configuration_string;

    for (auto i = configuration_.begin(); i != configuration_.end(); ++i) {
        char l_length = char(1 + i->first.length() + i->second.length());
        configuration_string.push_back(l_length);
        configuration_string.append(i->first);
        configuration_string.push_back('=');
        configuration_string.append(i->second);
    }
    configuration_string.push_back('\0');

    is_successful = option_impl::serialize(_to);
    if (is_successful) {
        is_successful = _to->serialize(
                reinterpret_cast<const uint8_t*>(configuration_string.c_str()),
                uint32_t(configuration_string.length()));
    }

    return is_successful;
}

bool configuration_option_impl::deserialize(vsomeip_v3::deserializer *_from) {
    if (!option_impl::deserialize(_from)) {
        VSOMEIP_WARNING << __func__ << "Could not deserialize Option header.";
        return false;
    }

    // Length contains reserved byte.
    const uint32_t string_length = length_ - 1;
    std::string raw_string(string_length, 0);

    if (string_length == 0) {
        VSOMEIP_WARNING << "Configuration Option: Invalid String length.";
        return false;
    }

    if (!_from->deserialize(raw_string, string_length)) {
        VSOMEIP_WARNING << "Configuration Option: Could not deserialize Configuration String.";
        return false;
    }

    uint32_t substring_size_index = 0;
    uint8_t substring_size = static_cast<uint8_t>(raw_string[substring_size_index]);
    while (substring_size != 0) {
        const uint32_t substring_begin_index = substring_size_index + 1;
        const uint32_t substring_end_index = substring_begin_index + substring_size;

        if (substring_end_index > string_length) {
            VSOMEIP_WARNING << "Configuration Option: Invalid Configuration substring size.";
            return false;
        }

        const char* const sub_string = raw_string.data() + substring_begin_index;
        uint32_t equal_sign_index = 0;
        if (sub_string[0] == 0x3D) {
            VSOMEIP_WARNING << "Configuration Option: Substring of Configuration Option starts with '='.";
            return false;
        }
        for (uint32_t i = 0; i < substring_size; ++i) {
            const char c = sub_string[i];
            if (c < 0x20 || c > 0x7E) {
                VSOMEIP_WARNING << "Configuration Option: Non ASCII character in Configuration Option key.";
                return false;
            }
            if (c == 0x3D && equal_sign_index == 0) {
                equal_sign_index = i;
                break;
            }
        }

        // No '=' means that key is present.
        if (equal_sign_index == 0) {
            configuration_.emplace(std::make_pair(std::string(sub_string, substring_size), std::string()));
        } else {
            configuration_.emplace(
                std::make_pair(
                    std::string(sub_string, equal_sign_index),
                    std::string(sub_string + equal_sign_index + 1, substring_size - equal_sign_index - 1)
                )
            );
        }

        substring_size_index = substring_end_index;
        substring_size = static_cast<uint8_t>(raw_string[substring_size_index]);
    }

    if (substring_size_index < string_length - 1) {
        VSOMEIP_WARNING << "Configuration Option: String length exceeds escape character.";
    }

    return true;
}

} // namespace sd
} // namespace vsomeip_v3
