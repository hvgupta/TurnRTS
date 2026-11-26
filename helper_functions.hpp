#ifndef _HELPER
#define _HELPER 1

#pragma once

#include <concepts>
#include <functional>
#include <unordered_map>
#include <vector>

template <typename K, typename V>
  requires std::equality_comparable<K> && std::equality_comparable<V> &&
           requires(K k, V v) {
             { std::hash<K>{}(k) } -> std::convertible_to<std::size_t>;
             { std::hash<V>{}(v) } -> std::convertible_to<std::size_t>;
           }
std::unordered_map<V, std::vector<K>>
invert_map(const std::unordered_map<K, V> &map) {
  std::unordered_map<V, std::vector<K>> inv_map;
  for (const std::pair<K, V> &p : map) {
    inv_map[p.second].push_back(p.first);
  }

  return inv_map;
}

#endif