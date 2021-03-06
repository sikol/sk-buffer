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

#ifndef SK_BUFFER_CIRCULAR_BUFFER_HXX_INCLUDED
#define SK_BUFFER_CIRCULAR_BUFFER_HXX_INCLUDED

#include <algorithm>
#include <array>
#include <cassert>
#include <ranges>
#include <span>

#include "sk/buffer/buffer.hxx"

namespace sk {

    /*************************************************************************
     *
     * circular_buffer: a fixed-size buffer which can wrap around when it
     * reaches the end of the buffer.  Unlike fixed_buffer, which becomes
     * useless and has to be reset once the entire buffer has been written and
     * read once, circular_buffer can be continually read and written forever.
     * However, it can never contain more data at once than its fixed size.
     *
     */

    template <typename Char, std::size_t buffer_size = 4096>
    struct circular_buffer {
        using array_type = std::array<Char, buffer_size + 1>;
        using size_type = std::size_t;
        using value_type = Char;
        using const_value_type = std::add_const_t<Char>;

        // Create a new, empty buffer.
        circular_buffer() = default;

        // circular_buffer cannot be copied or moved, because the buffer
        // contains the entire std::array<>.  In principle we could allow
        // copying, but copying a buffer is almost certainly a programming
        // error.

        circular_buffer(circular_buffer const &) = delete;
        circular_buffer &operator=(circular_buffer const &) = delete;
        circular_buffer(circular_buffer &&) = delete;
        circular_buffer &operator=(circular_buffer &&) = delete;

        // The data stored in this buffer.
        array_type data;

        // Reset the buffer.
        auto clear() -> void {
            read_pointer = data.begin();
            write_pointer = data.begin();
        }

        /*
         * read_pointer and write_pointer store the current locations where
         * we can read data from resp. write data to.  The readable range
         * is from the read pointer to one before write pointer, and the
         * writable range is from the write pointer to one before the read
         * pointer.
         *
         * The two pointers can be in one of the following states:
         *
         *  v begin                    end v
         * [...............................]
         *             ^--write ptr
         *             ^--read ptr
         *
         *      The read and write pointers point to the same location.
         *      This means we can write to the entire buffer, and no data
         *      can be read.  This is how the buffer starts out.
         *
         *  v begin                    end v
         * [...............................]
         *   read ptr -^  ^- write ptr
         *
         *      3 bytes of data have been written to the buffer, and now the
         *      write pointer is ahead of the read pointer.
         *
         *  v begin                    end v
         * [...............................]
         *       ^     ^- read ptr
         *        \ write ptr
         *
         *      Enough data has been written to the buffer that the write
         *      pointer has wrapped around.
         *
         *  v begin                    end v
         * [...............................]
         *            ^^- read ptr
         *             \ write ptr
         *
         *      The buffer is full; all data can be read and no data can be
         *      written.
         *
         */

        // The current read pointer.  Data from the read pointer until one
        // object prior to the write pointer can be read.  If
        // read_pointer == write_pointer, this means no data can be read.
        typename array_type::iterator read_pointer = data.begin();

        // The current write pointer.  Space from the write pointer to
        // the read pointer can be written to.  If read_pointer ==
        // write_pointer, this means the entire buffer can be written to.
        typename array_type::iterator write_pointer = data.begin();

        // Write data to the buffer.  Returns the number of objects written,
        // which might less than the size of the range if the buffer is full.
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
    };

    /*
     * circular_buffer::write()
     */
    template <typename Char, std::size_t buffer_size>
    template <std::ranges::contiguous_range InRange>
    auto circular_buffer<Char, buffer_size>::write(InRange &&buf) -> size_type
        requires std::same_as<
            const_value_type,
            std::add_const_t<std::ranges::range_value_t<InRange>>> {

        size_type bytes_written = 0;
        std::span<const_value_type> data_left{buf};

        for (auto &&range : writable_ranges()) {
            auto can_write =
                std::min(data_left.size(), std::ranges::size(range));
            std::ranges::copy(data_left.subspan(0, can_write),
                              std::ranges::begin(range));
            data_left = data_left.subspan(can_write);
            bytes_written += can_write;

            if (data_left.empty())
                break;
        }

        commit(bytes_written);
        return bytes_written;
    }

    /*
     * circular_buffer::writable_ranges()
     */
    template <typename Char, std::size_t buffer_size>
    auto circular_buffer<Char, buffer_size>::writable_ranges()
        -> std::vector<std::span<value_type>> {

        std::vector<std::span<value_type>> ret;
        auto theoretical_write_pointer = write_pointer;

        // If read ptr == write ptr, we can write to the entire buffer.
        if (read_pointer <= theoretical_write_pointer) {
            // Determine the last buffer position we can write to.
            auto wend = data.end();

            // If the read_pointer is at the start of the buffer, we have
            // to leave one empty byte at the end to stop the write pointer
            // running into the read pointer.
            if (read_pointer == data.begin())
                wend--;

            // Write between write_pointer and the end of the buffer.
            auto span = std::span<value_type>(
                theoretical_write_pointer,
                theoretical_write_pointer + (wend - theoretical_write_pointer));

            if (!span.empty())
                ret.push_back(span);

            // Set write_pointer past the data we write.
            theoretical_write_pointer +=
                static_cast<typename array_type::difference_type>(span.size());

            // write_pointer may be left pointing at end(); if there's
            // space left at the start of the buffer, wrap it now.
            if (theoretical_write_pointer == data.end() &&
                read_pointer != data.begin())
                theoretical_write_pointer = data.begin();
        }

        if (read_pointer > theoretical_write_pointer) {
            auto span = std::span<value_type>(theoretical_write_pointer,
                                              read_pointer - 1);
            if (!span.empty())
                ret.push_back(span);
        }

        return ret;
    }

    /*
     * circular_buffer::commit()
     */
    template <typename Char, std::size_t buffer_size>
    auto circular_buffer<Char, buffer_size>::commit(size_type n) -> size_type {
        auto bytes_left = n;
        size_type bytes_written = 0;

        // If read ptr == write ptr, we can write to the entire buffer.
        if (read_pointer <= write_pointer) {
            // Determine the last buffer position we can write to.
            auto wend = data.end();

            // If the read_pointer is at the start of the buffer, we have
            // to leave one empty byte at the end to stop the write pointer
            // running into the read pointer.
            if (read_pointer == data.begin())
                wend--;

            // Write between write_pointer and the end of the buffer.
            auto can_write =
                std::min(static_cast<typename array_type::size_type>(
                             wend - write_pointer),
                         bytes_left);
            assert(can_write >= 0);

            bytes_left -= can_write;
            bytes_written += can_write;

            // Set write_pointer past the data we write.
            write_pointer +=
                static_cast<typename array_type::difference_type>(can_write);

            // write_pointer may be left pointing at end(); if there's
            // space left at the start of the buffer, wrap it now.
            if (write_pointer == data.end() && read_pointer != data.begin())
                write_pointer = data.begin();

            // Fall through to check read_pointer > write_pointer, in case
            // we wrapped.
        }

        if (read_pointer > write_pointer) {
            // Write between write_pointer and read_pointer - 1.
            auto can_write =
                std::min(static_cast<typename array_type::size_type>(
                             read_pointer - data.begin() - 1),
                         bytes_left);
            assert(can_write >= 0);

            bytes_written += can_write;
            write_pointer +=
                static_cast<typename array_type::difference_type>(can_write);
            assert(write_pointer < read_pointer);
        }

        return bytes_written;
    }

    /*
     * circular_buffer::read()
     */
    template <typename Char, std::size_t buffer_size>
    template <std::ranges::contiguous_range InRange>
    auto circular_buffer<Char, buffer_size>::read(InRange &&buf) -> size_type
        requires std::same_as<value_type, std::ranges::range_value_t<InRange>> {

        size_type bytes_read = 0;
        std::span<value_type> data_left{buf};

        for (auto &&range : readable_ranges()) {
            auto can_read = std::min(data_left.size(), range.size());
            std::ranges::copy(range.subspan(0, can_read), data_left.begin());
            data_left = data_left.subspan(can_read);
            bytes_read += can_read;

            if (data_left.empty())
                break;
        }

        discard(bytes_read);
        return bytes_read;
    }

    /*
     * circular_buffer::readable_ranges()
     */
    template <typename Char, std::size_t buffer_size>
    auto circular_buffer<Char, buffer_size>::readable_ranges()
        -> std::vector<std::span<const_value_type>> {

        std::vector<std::span<const_value_type>> ret;

        // If read_pointer == write_pointer, the buffer is empty.
        if (read_pointer == write_pointer)
            return ret;

        auto theoretical_read_pointer = read_pointer;

        // If read_pointer > write_pointer, we can read from read_pointer
        // until the end of the buffer.
        if (theoretical_read_pointer > write_pointer) {
            auto can_read = static_cast<typename array_type::size_type>(
                data.end() - theoretical_read_pointer);
            auto span = std::span<const_value_type>(
                theoretical_read_pointer,
                theoretical_read_pointer +
                    static_cast<std::ptrdiff_t>(can_read));

            if (!span.empty())
                ret.push_back(span);

            theoretical_read_pointer += static_cast<ptrdiff_t>(can_read);

            // If read_pointer reached the end of the buffer, move it back to
            // the start.
            if (theoretical_read_pointer == data.end())
                theoretical_read_pointer = data.begin();

            // Fall through to check for read_pointer < write_pointer in case
            // we wrapped.
        }

        // If read_pointer < write_pointer, we can read from read_pointer
        // up to write_pointer.
        if (theoretical_read_pointer < write_pointer) {
            auto can_read = static_cast<typename array_type::size_type>(
                write_pointer - theoretical_read_pointer);
            assert(can_read >= 0);

            auto span = std::span<const_value_type>(
                theoretical_read_pointer,
                theoretical_read_pointer +
                    static_cast<std::ptrdiff_t>(can_read));

            if (!span.empty())
                ret.push_back(span);
            theoretical_read_pointer += static_cast<ptrdiff_t>(can_read);
        }

        assert(theoretical_read_pointer < data.end());
        return ret;
    }

    /*
     * circular_buffer::discard()
     */
    template <typename Char, std::size_t buffer_size>
    auto circular_buffer<Char, buffer_size>::discard(size_type n) -> size_type {

        // If read_pointer == write_pointer, the buffer is empty.
        if (read_pointer == write_pointer)
            return 0;

        auto bytes_left = n;
        size_type bytes_read = 0;

        // If read_pointer > write_pointer, we can read from read_pointer
        // until the end of the buffer.
        if (read_pointer > write_pointer) {
            auto can_read = std::min(
                bytes_left, static_cast<typename array_type::size_type>(
                                data.end() - read_pointer));

            bytes_left -= can_read;
            bytes_read += can_read;
            read_pointer += static_cast<ptrdiff_t>(can_read);

            // If read_pointer reached the end of the buffer, move it back to
            // the start.
            if (read_pointer == data.end())
                read_pointer = data.begin();

            // Fall through to check for read_pointer < write_pointer in case
            // we wrapped.
        }

        // If read_pointer < write_pointer, we can read from read_pointer
        // up to write_pointer.
        if (read_pointer < write_pointer) {
            auto can_read = std::min(
                bytes_left, static_cast<typename array_type::size_type>(
                                write_pointer - read_pointer));
            assert(can_read >= 0);

            read_pointer += static_cast<ptrdiff_t>(can_read);
            bytes_read += can_read;
        }

        assert(read_pointer < data.end());
        return bytes_read;
    }

} // namespace sk

#endif // SK_BUFFER_CIRCULAR_BUFFER_HXX_INCLUDED
