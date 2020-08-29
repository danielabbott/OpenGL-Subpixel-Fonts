#pragma once

#include "Assert.h"


namespace SubPixelFonts {

	// Throws exceptions instead of crashing when in release mode

	template <typename T>
	class SaferRawPointer {
	public:
		SaferRawPointer() : p(nullptr) {}
		SaferRawPointer(T* p_) : p(p_) {}

#define TEST() assert__(p, "Dereferenced null pointer");

		T& operator*() const {
			TEST()
				return *p;
		}
		T* operator->() const {
			TEST()
				return p;
		}

		bool is_null() const {
			return p == nullptr;
		}
		bool is_valid() const {
			return p != nullptr;
		}

		T* get() const {
			TEST()
				return p;
		}
	private:
		T* p;
	};

}
