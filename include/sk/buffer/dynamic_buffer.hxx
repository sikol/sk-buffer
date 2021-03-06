/*
 * Copyright (c) 2019, 2020, 2021 SiKol Ltd.
 *
 * Boost Software License - Version 1.0 - August 17th, 2003
 *
 * Permission is hereby granted, free of charge, to any person or organization
 * obtaining a copy of the software and accompanying documentation covered by
 * this license (the "Software") to use, reproduce, display, distribute,
 * execute, and transmit the Software, and to prepare derivative works of the
 * Software, and to permit third-parties to whom the Software is furnished to
 * do so, all subject to the following:
 *
 * The copyright notices in the Software and this entire statement, including
 * the above license grant, this restriction and the following disclaimer,
 * must be included in all copies of the Software, in whole or in part, and
 * all derivative works of the Software, unless such copies or derivative
 * works are solely in the form of machine-executable object code generated by
 * a source language processor.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
 * SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
 * FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef SK_BUFFER_DYNAMIC_BUFFER_HXX_INCLUDED
#define SK_BUFFER_DYNAMIC_BUFFER_HXX_INCLUDED

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <deque>
#include <ranges>
#include <span>
#include <type_traits>

#include "sk/buffer/buffer.hxx"
#include "sk/buffer/fixed_buffer.hxx"

namespace sk {

    /*************************************************************************
     *
     * dynamic_buffer: a buffer which grows and contracts dynamically as
     * required. Growth is managed efficiently so the data is never copied, and
     * there is no upper bound on the size of the buffer (other than available
     * memory).
     *
     * A dynamic buffer consists of a series of fixed_buffers, which are
     * contiguous ranges of objects of a fixed size, organised into a deque. New
     * fixed_buffers will be automatically added and removed as the buffer is
     * used.
     *
     */

    // Calculate how large a buffer extent should be if we want to use
    // a specific number of bytes.

    template <typename Char> constexpr auto extent_size_from_bytes(int nbytes) {
        return nbytes / sizeof(Char);
    }

    template <typename Char, std::size_t extent_bytes = 4096>
    struct dynamic_buffer {
        using size_type = std::size_t;
        using value_type = Char;
        using const_value_type = std::add_const_t<Char>;

        static constexpr std::size_t extent_size =
            extent_size_from_bytes<value_type>(extent_bytes);
        using extent_type = fixed_buffer<value_type, extent_size>;
        using extent_list_type = std::deque<extent_type>;

        // The minimum amount of space to keep available for writing; if the
        // write window is small than this, we will allocate a new extent.
        // This ensures users writing directly into the buffer have sufficient
        // space to write into.
        static constexpr std::size_t minfree = extent_size / 2;

        // Create a new, empty buffer with a single extent.
        dynamic_buffer() = default;

        // dynamic_buffer is not copyable, but can be moved.
        dynamic_buffer(dynamic_buffer const &) = delete;
        dynamic_buffer &operator=(dynamic_buffer const &) = delete;
        dynamic_buffer(dynamic_buffer &&) = default;
        dynamic_buffer &operator=(dynamic_buffer &&) = default;

        // The extents in this buffer.
        extent_list_type extents;

        // Index of the first buffer we can write data into.
        typename extent_list_type::size_type write_pointer = 0;

        // Write data to the buffer.  All of the data will be written, and the
        // buffer will be expanded to fit the data if necessary.
        template <std::ranges::contiguous_range Range>
        auto write(Range &&) -> size_type requires std::same_as<
            const_value_type,
            std::add_const_t<std::ranges::range_value_t<Range>>>;

        // Read data from the buffer.  As much data will be read as possible,
        // and the number of objects read will be returned.  If the return value
        // is less than the requested number of objects, the buffer is now
        // empty.
        template <std::ranges::contiguous_range Range>
        auto read(Range &&) -> size_type
            requires std::same_as<value_type,
                                  std::ranges::range_value_t<Range>>;

        // Return a list of ranges which represent data in the buffer
        // which can be read.  Writing data to the buffer will not invalidate
        // the ranges.
        //
        // After reading the data, discard() should be called to remove the
        // data from the buffer.
        auto readable_ranges() -> std::vector<std::span<const_value_type>>;

        // Discard up to n bytes of readable data from the start of the buffer.
        // Returns the number of bytes discarded.
        auto discard(size_type n) -> size_type;

        // Return a list of ranges representing space in the buffer
        // which can be written to.  After writing the data, commit() should be
        // called to mark the space as used.
        auto writable_ranges() -> std::vector<std::span<value_type>>;

        // Mark n bytes of previously empty space as containing data.
        auto commit(size_type n) -> size_type;

        // Ensure that at least minfree objects of write space is available.
        auto ensure_minfree() -> void {
            // Add more space if needed.
            if (extents.empty() ||
                extents.back().write_window.size() < minfree) {

                extents.emplace_back();
                // Make sure write_pointer doesn't point at an empty extent.
                if (extents[write_pointer].write_window.size() == 0)
                    ++write_pointer;
            }
        }

      private:
        // Remove the first element of the buffer.
        auto remove_front() -> void;
    };

    static_assert(buffer<dynamic_buffer<char>>);

    template <typename Char, std::size_t extent_size>
    auto dynamic_buffer<Char, extent_size>::remove_front() -> void {
        assert(!extents.empty());
        assert(write_pointer > 0 || extents.front().write_window.size() == 0);

        if (write_pointer > 0)
            --write_pointer;

        extents.pop_front();
    }

    template <typename Char, std::size_t extent_size>
    auto dynamic_buffer<Char, extent_size>::writable_ranges()
        -> std::vector<std::span<Char>> {
        std::vector<std::span<Char>> ret;

        // Make sure we always return a reasonable amount of writable space.
        ensure_minfree();

        assert(write_pointer >= 0 && write_pointer < extents.size());

        for (auto i = write_pointer, end = extents.size(); i < end; ++i) {
            // Every extent from write_pointer onwards must have free space.
            assert(extents[i].write_window.size() > 0);

            // Every extent aside from the current write pointer must be empty,
            // or else we have written data in front of the pointer without
            // adjusting it, which is a bug.
            assert(i == write_pointer ||
                   (extents[i].write_window.size() == extent_size));

            // Add this extent to the range list.
            ret.push_back(extents[i].write_window);
        }

        return ret;
    }

    template <typename Char, std::size_t extent_size>
    auto dynamic_buffer<Char, extent_size>::commit(std::size_t n) -> size_type {
        size_type left = n;

        if (n == 0)
            return 0;

        assert(write_pointer >= 0 && write_pointer < extents.size());

        assert(write_pointer <
               static_cast<typename extent_list_type::size_type>(
                   std::numeric_limits<
                       typename extent_list_type::difference_type>::max()));

        for (;;) {
            // If we reach the end of the buffer, we have run out of data
            // to commit.  Trying to commit more data than exists in the buffer
            // is almost certainly a mistake on the caller's part, so rather
            // than returning a short commit, just abort.
            assert(write_pointer < extents.size());

            // Commit as much data as possible in this extent.
            auto m = extents[write_pointer].commit(left);

            // Ensure we committed at least 1 object.  If not, that means our
            // writer pointer was pointing at the wrong extent.
            assert(m > 0);

            // If we committed all of it, return.
            left -= m;
            if (left == 0) {
                // Call ensure_minfree() here to avoid the situation where we
                // committed exactly the available size of the last extent,
                // which will leave write_pointer pointing at a full extent.
                ensure_minfree();
                return n;
            }

            // Move to the next extent and continue.
            ++write_pointer;
        }
    }

    template <typename Char, std::size_t extent_size>
    template <std::ranges::contiguous_range Range>
    auto dynamic_buffer<Char, extent_size>::write(Range &&data) -> size_type
        requires std::same_as<
            const_value_type,
            std::add_const_t<std::ranges::range_value_t<Range>>> {

        std::span<std::add_const_t<std::ranges::range_value_t<Range>>> buf =
            data;

        if (buf.size() == 0)
            return 0;

        assert(write_pointer >= 0 && write_pointer <= extents.size());

        for (;;) {
            // If we're at the end of the buffer, add another extent to the end.
            if (write_pointer == extents.size())
                extents.emplace_back();

            // Write as much data as possible.
            auto n = extents[write_pointer].write(buf);
            buf = buf.subspan(n);

            // If we wrote all of it, return.
            if (buf.size() == 0) {
                // Make sure we don't leave write_pointer pointing at a
                // full extent.
                ensure_minfree();
                return std::ranges::size(data);
            }

            // Move to the next extent and try again.
            ++write_pointer;
        }
    }

    template <typename Char, std::size_t extent_size>
    auto dynamic_buffer<Char, extent_size>::readable_ranges()
        -> std::vector<std::span<const_value_type>> {
        std::vector<std::span<const_value_type>> ret;

        for (auto const &ext : extents) {
            if (ext.read_window.size() == 0)
                continue;

            ret.push_back(ext.read_window);
        }

        return ret;
    }

    template <typename Char, std::size_t extent_size>
    auto dynamic_buffer<Char, extent_size>::discard(size_type n) -> size_type {
        std::size_t discards = 0;
        std::size_t nleft = n;

        for (;;) {
            if (extents.empty())
                return discards;

            auto &front = extents.front();

            // If there's no data to discard, return.
            if (front.read_window.size() == 0)
                return discards;

            // Discard as much as possible.
            auto m = front.discard(nleft);
            nleft -= m;
            discards += m;

            // If the extent we read from is dead, remove it.
            if (front.read_window.size() == 0 && front.write_window.size() == 0)
                remove_front();

            // If we discarded everything, return.
            if (discards == n)
                return discards;
        }
    }

    template <typename Char, std::size_t extent_size>
    template <std::ranges::contiguous_range Range>
    auto dynamic_buffer<Char, extent_size>::read(Range &&data) -> size_type
        requires std::same_as<value_type, std::ranges::range_value_t<Range>> {

        std::span<std::ranges::range_value_t<Range>> buf = data;
        std::size_t bytes_read = 0;

        if (buf.size() == 0)
            return 0;

        for (;;) {
            if (extents.empty())
                return bytes_read;

            auto &front = extents.front();

            // If there's no data to read, return.
            if (front.read_window.size() == 0)
                return bytes_read;

            // Read as much as possible.
            auto n = front.read(buf);
            buf = buf.subspan(n);
            bytes_read += n;

            // If the extent we read from is dead, remove it.
            if (front.read_window.size() == 0 && front.write_window.size() == 0)
                remove_front();

            // If we read everything, return.
            if (bytes_read == std::ranges::size(data))
                return bytes_read;
        }
    }

} // namespace sk

#endif // SK_BUFFER_DYNAMIC_BUFFER_HXX_INCLUDED
