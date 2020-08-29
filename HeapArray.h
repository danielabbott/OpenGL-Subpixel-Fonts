#pragma once

#include <cstdint>
#include <functional>
#include "Assert.h"

namespace SubPixelFonts {

	// Non-resizeable array stored on the heap.
	// Includes bounds checking when using at()

	template <typename T>
	class HeapArray {
		inline static const std::string EXCEPTION_OUT_OF_BOUNDS = "Array index out of bounds";
	public:
		HeapArray(uintptr_t n) {
			ptr = new T[n];
			data_size = n;
		}

		using Deleter = std::function<void(T*)>;

		// Will call delete[] on p when destroyed
		HeapArray(T* p, unsigned int n) : ptr(p), data_size(n) {
		}

		HeapArray(T* p, unsigned int n, Deleter del) : ptr(p), data_size(n), deleter(del) {
		}

		~HeapArray() {
			if (ptr) {
				if (deleter.has_value()) {
					(deleter.value())(ptr);
				}
				else {
					delete[] ptr;
				}
			}
		}

		T* get() const {
			return ptr;
		}

		T& at(uintptr_t i) {
			assert__(i < data_size, EXCEPTION_OUT_OF_BOUNDS);
			return ptr[i];
		}

		T const& at(uintptr_t i) const {
			assert__(i < data_size, EXCEPTION_OUT_OF_BOUNDS);
			return ptr[i];
		}

		uintptr_t size() const {
			return data_size;
		}

		HeapArray(const HeapArray&) = delete;
		HeapArray& operator=(const HeapArray&) = delete;

		HeapArray(HeapArray&& other) noexcept : ptr(other.ptr), data_size(other.data_size), deleter(other.deleter) {
			other.ptr = nullptr;
			other.data_size = 0;
			other.deleter = std::nullopt;
		}
		HeapArray& operator=(HeapArray&& other) noexcept {
			ptr = other.ptr;
			data_size = other.data_size;
			deleter = other.deleter;

			other.ptr = nullptr;
			other.data_size = 0;
			other.deleter = std::nullopt;
			return *this;
		}
	private:
		T* ptr;
		uintptr_t data_size; // Number of elements
		std::optional<Deleter> deleter = std::nullopt;
	};
}
