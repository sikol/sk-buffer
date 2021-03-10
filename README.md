# C++ 20 I/O buffers

This library provides data buffer types based on C++ concepts and ranges:

* Fixed-size buffer.
* Circular buffer.
* Dynamically-expanding buffer.
* Range adapters to allow ranges to be treated as buffers.
* Adapters to allow buffers to be runtime-polymorphic.

The buffers are intended to be suitable for efficient zero-copy I/O.

**WARNING**: This library is a work-in-progress and almost certainly
contains many bugs.  

## Installation

Copy header files into your source tree. If your project uses CMake, 
you can add the entire project as a subdirectory (e.g., with Git
submodules) and link with the sk-buffer library.

## API reference

### Concepts

* `sk::basic_buffer<T>`:
    * A base type for all buffers.
    * Type `T::value_type`: type of object stored in the buffer.
    * Type `T::const_value_type`: `std::add_const_t<value_type>`
    * Type `T::size_type`: integral type that can represent the size of the buffer.
    * Fn `T::clear()`: Reset the buffer to its default, empty state, discarding
      any data it contains. 

* `sk::readable_buffer<T>`:
    * A buffer that can be read from.
    * Fn `T::readable_ranges() -> std::vector<std::span<const_value_type>>`:
      Return a list of contiguous ranges which contain data in the buffer.
      Data in the ranges can be copied or passed to data-consuming functions.
    * Fn `T::discard(size_type n) -> size_type`: Discard up to `n` objects
      from the start of the buffer.  Returns the number of objects discarded.
    * Fn `T::read(std::ranges::contiguous_range &r) -> size_type`:
      Copy data from the buffer into `r`, then discard the copied objects.
      Returns the number of objects copied.

* `sk::writable_buffer<T>`:
    * A buffer that can be written to.
    * Fn `T::writable_ranges() -> std::vector<std::span<value_type>>`:
      Return a list of contiguous ranges which refer to empty space in the buffer.
      Data can be written to the empty space.
    * Fn `T::commit(size_type n)`: Mark up to `n` objects at the start of the
      buffer's empty space as readable data.  Returns the number of objects
      committed.
    * Fn `T::write(std::ranges::contiguous_range const &r) -> size_type`:
      Copy objects from `r` into the buffer, then commit the number of
      objects copied.  Returns the number of objects copied.

* `buffer<T>`: `readable_buffer<T> && writable_buffer<T>`

### Implementations

#### Compile-time polymorphic buffers

* `sk::fixed_buffer<T, std::size_t N>`: A fixed-size buffer that can contain `N` 
  objects of type `T`.  Writing data to the buffer fills it up; once the entire
  buffer has been written to, the buffer becomes useless until it is reset
  using `clear()`.

* `sk::circular_buffer<T, std::size_t N>`: A fixed-size circular buffer that can
  contain `N` objects of type `T`.  Unlike `fixed_buffer`, the circular buffer
  can be written to forever.  However, it can never contain more than `N` objects
  at once.

* `sk::dynamic_buffer<T, std::size_t N = 4096>`: A dynamic buffer that can contain
  an unlimited number of objects of type `T`.  The buffer can be written to
  forever without reading from it, subject to available memory.
  Objects are allocated in blocks of `N` bytes.

* `sk::readable_range_buffer<std::ranges::contiguous_range R>`: A buffer adapter
  that exposes a contiguous range as a readable buffer.  To create a readable
  buffer from a range `r`, use `sk::make_readable_range_buffer(r)`.

* `sk::writable_range_buffer<std::ranges::contiguous_range R>`: A buffer adapter
  that exposes a contiguous range as a writable buffer.  To create a writable
  buffer from a range `r`, use `sk::make_writable_range_buffer(r)`.


#### Runtime-polymorphic buffers

* `sk::pmr_readable_buffer<T>`: An abstract base class that represents a 
  runtime-polymorphic readable buffer.  This provides the same interface
  as `sk::readable_buffer<T>` except that all ranges are be `std::span`.

* `sk::pmr_writable_buffer<T>`: An abstract base class that represents a 
  runtime-polymorphic writable buffer.  This provides the same interface
  as `sk::writable_buffer<T>` except that all ranges are be `std::span`.

* `sk::pmr_buffer<T>`: An abstract base class that represents a 
  runtime-polymorphic buffer which is both readable and writable.  This
  provides the same interface as `sk::buffer<T>` except that all ranges
  are be `std::span`.

To create a runtime-polymorphic buffer from an existing buffer `b`,
use `sk::make_pmr_buffer_adapter(b)`.  Note that the returned buffer
adapter object holds a reference to `b` for its lifetime.
