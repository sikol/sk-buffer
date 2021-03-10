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

#include <catch.hpp>

#include "sk/buffer/pmr_buffer.hxx"
#include "sk/buffer/range_buffer.hxx"

TEST_CASE("pmr_buffer") {
    std::string input_string("testing");
    auto range_buffer = sk::make_readable_range_buffer(input_string);

    auto pmr_readable_buffer = sk::make_pmr_buffer_adapter(range_buffer);

    std::string output_string(input_string.size(), 'X');
    pmr_readable_buffer.read(output_string);

    REQUIRE(output_string == input_string);
}

TEST_CASE("pmr_writable_buffer") {
    std::string input_string("testing");

    std::string output_string(input_string.size(), 'X');
    auto range_buffer = sk::make_writable_range_buffer(output_string);

    auto pmr_writable_buffer = sk::make_pmr_buffer_adapter(range_buffer);
    pmr_writable_buffer.write(input_string);

    REQUIRE(output_string == input_string);
}
