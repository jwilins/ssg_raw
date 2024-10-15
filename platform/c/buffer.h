/*
 *   Memory ownership semantics
 *
 */

#pragma once

import std.compat;

struct BYTE_BUFFER_BORROWED : public std::span<const uint8_t> {
	using span::span;

	template <typename T, size_t N> BYTE_BUFFER_BORROWED(std::span<T, N> val) :
		span(reinterpret_cast<const uint8_t *>(val.data()), val.size_bytes()) {
	}
};

template <
	typename ConstOrNonConstByte
> struct BYTE_BUFFER_CURSOR : public std::span<ConstOrNonConstByte> {
	using std::span<ConstOrNonConstByte>::span;

	template <typename T> using transfer_const = std::conditional_t<
		std::is_const_v<ConstOrNonConstByte>, const T, T
	>;

	size_t cursor = 0;

	// Required to work around a C26495 false positive, for some reason?
	BYTE_BUFFER_CURSOR(const std::span<ConstOrNonConstByte> other) :
		std::span<ConstOrNonConstByte>(other) {
	}

	// Reads up to [n] contiguous values of type T from the active cursor
	// position if possible. If the function returns a valid span, all [n]
	// objects are safe to access.
	template <typename T> std::optional<std::span<transfer_const<T>>> next(
		size_t n = 1
	) {
		const auto cursor_new = (cursor + (sizeof(T) * n));
		if((cursor_new > this->size()) || (cursor_new < cursor)) {
			return std::nullopt;
		}
		[[gsl::suppress(type.1)]]
		auto ret = std::span<transfer_const<T>>{
			reinterpret_cast<transfer_const<T> *>(this->data() + cursor), n
		};
		cursor = cursor_new;
		return ret;
	}
};

// Same semantics as the underlying unique_ptr: Can be either allocated or
// empty.
struct BYTE_BUFFER_OWNED : public std::unique_ptr<uint8_t[]> {
private:
	size_t size_;

public:
	// Creates an empty buffer, with no allocation.
	BYTE_BUFFER_OWNED(std::nullptr_t null = nullptr) noexcept :
		std::unique_ptr<uint8_t[]>(null),
		size_(0) {
	}

	// Tries to allocate [size] bytes, and leaves the buffer empty on failure.
	BYTE_BUFFER_OWNED(size_t size) :
		std::unique_ptr<uint8_t[]>(new (std::nothrow) uint8_t[size]),
		size_(get() ? size : 0) {
	}

	auto size() const {
		return size_;
	}

	// Borrows a buffer with an immutable cursor.
	BYTE_BUFFER_CURSOR<const uint8_t> cursor() const {
		return { get(), size() };
	}

	// Borrows a buffer with a mutable cursor.
	BYTE_BUFFER_CURSOR<uint8_t> cursor_mut() {
		return { get(), size() };
	}
};

using BYTE_BUFFER_GROWABLE = std::vector<uint8_t>;
