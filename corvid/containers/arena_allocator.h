// Corvid20: A general-purpose C++20 library extending std.
// https://github.com/stevensudit/Corvid20
//
// Copyright 2022-2023 Steven Sudit
//
// Licensed under the Apache License, Version 2.0(the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#pragma once
#include "containers_shared.h"

namespace corvid { inline namespace container { namespace arena {

// Arena implemented as a singly-linked list of blocks.
//
// To use:
// 1. Create an `extensible_arena` with a given capacity and place it in your
// class.
// 2. Specialize containers with `arena_allocator` as the allocator.
// 3. Ensure that an `extensible_arena::scope` is created in each function that
// needs to allocate.
//
// Allocates new blocks as needed, chaining them together. Block size is
// constant, except when it must be enlarged to satisfy an allocation. Only
// frees when the entire arena is destroyed, and does not call destructors.
class extensible_arena {
  struct list_node;
  struct list_node_deleter {
    void operator()(list_node* node) const noexcept {
      node->~list_node();
      delete[] reinterpret_cast<char*>(node);
    }
  };
  using pointer = std::unique_ptr<list_node, list_node_deleter>;

  // Points to head owned by container. Use `extensible_arena_scope` to
  // install.
  thread_local static pointer* tls_head;

  struct list_node {
    size_t capacity{};
    size_t size{};
    pointer next{};
    std::byte data[1];

    // Make a new node of `capacity`.
    static pointer make(size_t capacity) {
      auto node = pointer{
          new (new char[sizeof(list_node) + capacity - 1]) list_node{}};
      node->capacity = capacity;
      return node;
    }

    // Allocate a block of size `n` with `align` alignment. If no room, returns
    // `nullptr`.
    void* allocate(size_t n, size_t align) noexcept {
      auto start = (size + align - 1) & ~(align - 1);
      auto past = start + n;
      if (past > capacity) return nullptr;
      size = past;
      return data + start;
    }
  };

  // Allocate a block of size `n` with `align` alignment. If no room at `head`,
  // replaces with new block, chaining the rest.
  static void* allocate(pointer& head, size_t n, size_t align) {
    if (auto start = head->allocate(n, align)) return start;
    auto new_head = list_node::make(std::min(head->capacity, n));
    new_head->next = std::move(head);
    head = std::move(new_head);
    return head->allocate(n, align);
  }

  pointer head_;

public:
  explicit extensible_arena(size_t capacity) noexcept
      : head_{list_node::make(capacity)} {}

  // Uses tls_head, per scope.
  static void* allocate(size_t n, size_t align) {
    assert(tls_head);
    return allocate(*tls_head, n, align);
  }

  class scope {
  public:
    explicit scope(extensible_arena& arena) noexcept : old_head(&arena.head_) {
      tls_head = &arena.head_;
    }

    ~scope() noexcept { tls_head = old_head; }

  private:
    pointer* old_head;
  };

  // Replaces both `exensible_arena` and `scope` with a fake.
  struct fake {
    explicit fake(const auto&) noexcept {}
  };
};

template<typename T>
class arena_allocator {
  static_assert(std::is_same_v<T, std::remove_cv_t<T>>);

public:
  using value_type = T;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using propagate_on_container_move_assignment = std::true_type;

  constexpr arena_allocator() noexcept = default;
  constexpr arena_allocator(const arena_allocator&) noexcept = default;
  template<class U>
  constexpr arena_allocator(const arena_allocator<U>&) noexcept {}

  constexpr ~arena_allocator() = default;

  // Allocates a block of memory suitable for an array of `n` objects of type
  // `T`, using the scoped `extensible_arena`.
  [[nodiscard]] constexpr T* allocate(std::size_t n) {
    return static_cast<T*>(
        extensible_arena::allocate(n * sizeof(T), alignof(T)));
  }

  // TODO: Maybe assert here because we don't want the destructor to run.
  constexpr void deallocate(T*, std::size_t) {}
};

// TODO: Consider std::scoped_allocator_adaptor

}}} // namespace corvid::container::arena