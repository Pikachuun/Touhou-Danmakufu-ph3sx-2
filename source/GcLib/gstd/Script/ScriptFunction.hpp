#pragma once

#include "../../pch.h"

#include "../Logger.hpp"

#include "LightweightVector.hpp"
#include "Value.hpp"

namespace gstd {
	class script_engine;
	class script_machine;

	typedef value (*callback)(script_machine* machine, int argc, const value* argv);

	//Breaks IntelliSense for some reason
#define DNH_FUNCAPI_DECL_(fn) static gstd::value fn (gstd::script_machine* machine, int argc, const gstd::value* argv)
#define DNH_FUNCAPI_(fn) gstd::value fn (gstd::script_machine* machine, int argc, const gstd::value* argv)

	struct function {
		const char* name;
		callback func;
		int arguments;

		function(const char* name_, callback func_) : function(name_, func_, 0) {};
		function(const char* name_, callback func_, int argc) : name(name_), 
			func(func_), arguments(argc) {};
	};
	struct constant {
		const char* name;
		type_data::type_kind type;
		uint64_t data;

		constant(const char* name_, int d_int) : constant(name_, (int64_t)d_int) {};
		constant(const char* name_, int64_t d_int);
		constant(const char* name_, double d_real);
		constant(const char* name_, wchar_t d_char);
		constant(const char* name_, bool d_bool);
	};

	class BaseFunction {
	public:
		static type_data::type_kind _type_test_promotion(type_data* type_l, type_data* type_r);
		static bool _type_assign_check(script_machine* machine, const value* v_src, const value* v_dst);
		static bool _type_assign_check_no_convert(script_machine* machine, const value* v_src, const value* v_dst);

		static value __script_perform_op_array(const value* v_left, const value* v_right, value(*func)(int, const value*));

		inline static double _fmod2(double i, double j);
		inline static int64_t _mod2(int64_t i, int64_t j);

		static value* _value_cast(value* val, type_data::type_kind kind);

		static bool _index_check(script_machine* machine, type_data* arg0_type, size_t arg0_size, int index);
		static bool _append_check(script_machine* machine, type_data* arg0_type, type_data* arg1_type);
		static bool _append_check_no_convert(script_machine* machine, type_data* arg0_type, type_data* arg1_type);

		static value _create_empty(type_data* type);

		//---------------------------------------------------------------------

		static value _script_add(int argc, const value* argv);
		DNH_FUNCAPI_DECL_(add);
		static value _script_subtract(int argc, const value* argv);
		DNH_FUNCAPI_DECL_(subtract);
		static value _script_multiply(int argc, const value* argv);
		DNH_FUNCAPI_DECL_(multiply);
		static value _script_divide(int argc, const value* argv);
		DNH_FUNCAPI_DECL_(divide);
		static value _script_fdivide(int argc, const value* argv);
		DNH_FUNCAPI_DECL_(fdivide);
		static value _script_remainder_(int argc, const value* argv);
		DNH_FUNCAPI_DECL_(remainder_);
		static value _script_negative(int argc, const value* argv);
		DNH_FUNCAPI_DECL_(negative);
		static value _script_power(int argc, const value* argv);
		DNH_FUNCAPI_DECL_(power);
		static value _script_compare(int argc, const value* argv);
		DNH_FUNCAPI_DECL_(compare);
		static value _script_not_(int argc, const value* argv);
		DNH_FUNCAPI_DECL_(not_);

		DNH_FUNCAPI_DECL_(modc);
		DNH_FUNCAPI_DECL_(predecessor);
		DNH_FUNCAPI_DECL_(successor);

		DNH_FUNCAPI_DECL_(length);
		DNH_FUNCAPI_DECL_(resize);

		static const value* index(script_machine* machine, int argc, const value* argv);

		DNH_FUNCAPI_DECL_(slice);
		DNH_FUNCAPI_DECL_(erase);

		DNH_FUNCAPI_DECL_(append);
		DNH_FUNCAPI_DECL_(concatenate);

		DNH_FUNCAPI_DECL_(round);
		DNH_FUNCAPI_DECL_(truncate);
		DNH_FUNCAPI_DECL_(ceil);
		DNH_FUNCAPI_DECL_(floor);

		static value _script_absolute(int argc, const value* argv);
		DNH_FUNCAPI_DECL_(absolute);

		DNH_FUNCAPI_DECL_(bitwiseNot);
		DNH_FUNCAPI_DECL_(bitwiseAnd);
		DNH_FUNCAPI_DECL_(bitwiseOr);
		DNH_FUNCAPI_DECL_(bitwiseXor);
		DNH_FUNCAPI_DECL_(bitwiseLeft);
		DNH_FUNCAPI_DECL_(bitwiseRight);

		DNH_FUNCAPI_DECL_(typeOf);
		DNH_FUNCAPI_DECL_(typeOfElem);

		DNH_FUNCAPI_DECL_(assert_);
		DNH_FUNCAPI_DECL_(script_debugBreak);
	};
}